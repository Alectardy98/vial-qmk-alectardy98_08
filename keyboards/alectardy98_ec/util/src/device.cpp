// SPDX-License-Identifier: GPL-3.0-or-later
#include "device.h"
#include "boardprofile.h"
#include "settings.h"
#include "calibration.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QStandardPaths>
#include <hidapi.h>
#include <lzma.h>
#include <stdexcept>
namespace ec {
namespace {
[[noreturn]] void error(const QString &s) {
    throw std::runtime_error(s.toStdString());
}
quint32 little32(const QByteArray &b) {
    if (b.size() < 4)
        error("Truncated Vial integer");
    return quint8(b[0]) | (quint32(quint8(b[1])) << 8) | (quint32(quint8(b[2])) << 16) |
           (quint32(quint8(b[3])) << 24);
}
class HidTransport : public Transport {
    hid_device *handle = nullptr;
    QLockFile lock;

  public:
    explicit HidTransport(const QByteArray &path)
        : lock(QDir::tempPath() + "/qmk-ec-util-" +
               QCryptographicHash::hash(path, QCryptographicHash::Sha256).toHex() + ".lock") {
        if (!lock.tryLock())
            error("Interface is in use by another QMK EC Utility process");
        handle = hid_open_path(path.constData());
        if (!handle)
            error("Cannot open HID: permissions, busy interface or disconnected device");
    }
    ~HidTransport() override {
        if (handle)
            hid_close(handle);
    }
    QByteArray exchange(const QByteArray &b) override {
        if (b.size() != 32)
            error("Invalid outgoing HID size");
        // Bounded drain; do not run two Raw HID clients simultaneously.
        unsigned char buffer[32];
        for (int i = 0; i < 8; ++i) {
            int n = hid_read_timeout(handle, buffer, 32, 0);
            if (n < 0)
                error("HID disconnected during drain");
            if (!n)
                break;
        }
        QByteArray framed(1, '\0');
        framed += b;
        if (hid_write(handle, reinterpret_cast<const unsigned char *>(framed.constData()), 33) != 33)
            error("HID write failed");
        int n = hid_read_timeout(handle, buffer, 32, 500);
        if (n != 32)
            error(n < 0 ? "HID disconnected" : "HID timeout/short response; close Vial and retry");
        return QByteArray(reinterpret_cast<char *>(buffer), 32);
    }
};
} // namespace
QByteArray VialReader::read(Transport &t, const QByteArray &request) {
    if (request.size() > 32)
        error("Oversized Vial request");
    QByteArray b = request;
    b.resize(32, '\0');
    auto reply = t.exchange(b);
    if (reply.size() != 32)
        error("Short Vial report");
    return reply;
}
DeviceInfo VialReader::identify(Transport &t, DeviceInfo info) {
    auto via = read(t, QByteArray(1, char(1)));
    if (quint8(via[0]) != 1 || wire::get16(via, 1) < 9 || wire::get16(via, 1) > 12)
        error("Unsupported VIA protocol");
    auto id = read(t, QByteArray::fromHex("fe00"));
    auto version = little32(id);
    if (version < 1 || version > 6)
        error("Invalid or unsupported Vial identity response; close Vial and other Raw HID clients, then retry");
    info.uid = id.mid(4, 8);
    if (info.uid == QByteArray(8, '\0') || info.uid == QByteArray(8, char(0xff)))
        error("Invalid Vial UID");
    if (const auto *profile=boardProfile(info.uid); profile && !profile->explicitCalibrationAvailable)
        error(profile->name + ": original Vial firmware identified. Universal calibration/storage port is not validated; no EC command was sent.");
    DeviceClient ec(t);
    auto dims = ec.hello();
    if(ec.simulated()||!ec.supportsExplicitCalibration())
        error("EC42 v2.9 calibration capability required; no write sent");
    auto build = ec.request(wire::Op::FirmwareInfo).payload;
    if (build.isEmpty() || build.size() > 20)
        error("Missing EC firmware build identity");
    if (build.endsWith(char(0)))
        build.chop(1);
    info.firmware = QString::fromLatin1(build);
    info.ecMinor=ec.minorVersion();
    if(const auto *profile=boardProfile(info.uid); profile && (profile->rows!=dims.first || profile->cols!=dims.second))
        error("EC matrix dimensions do not match the identified board profile");
    info.rows = dims.first;
    info.cols = dims.second;
    return info;
}
QByteArray VialReader::decompress(const QByteArray &packed) {
    if (packed.isEmpty() || packed.size() > 256 * 1024)
        error("Invalid compressed layout length");
    QByteArray output(4 * 1024 * 1024, '\0');
    size_t in = 0, out = 0;
    uint64_t limit = 16 * 1024 * 1024;
    auto rc = lzma_stream_buffer_decode(
        &limit, 0, nullptr, reinterpret_cast<const uint8_t *>(packed.constData()), &in, packed.size(),
        reinterpret_cast<uint8_t *>(output.data()), &out, output.size());
    if (rc != LZMA_OK || in != size_t(packed.size()))
        error("Invalid/oversized XZ layout definition");
    output.resize(out);
    return output;
}
QByteArray VialReader::definition(Transport &t) {
    quint32 size = little32(read(t, QByteArray::fromHex("fe01")));
    if (!size || size > 256 * 1024)
        error("Invalid embedded Vial definition size");
    QElapsedTimer deadline;
    deadline.start();
    QByteArray packed;
    for (quint32 page = 0; packed.size() < int(size); ++page) {
        if (deadline.elapsed() > 10000)
            error("Layout retrieval exceeded 10-second budget");
        QByteArray request = QByteArray::fromHex("fe02");
        request.append(char(page));
        request.append(char(page >> 8));
        auto part = read(t, request);
        packed += part.left(qMin(32, int(size) - int(packed.size())));
    }
    return decompress(packed);
}
quint32 VialReader::options(Transport &t) {
    auto p = read(t, QByteArray::fromHex("0202"));
    if (p.left(2) != QByteArray::fromHex("0202"))
        error("Invalid VIA layout-option reply");
    return wire::get32(p, 2);
}
QVector<int> VialReader::unpackOptions(const Layout &layout, quint32 packed) {
    QVector<int> result(layout.options.size(), 0);
    int consumed = 0;
    for (int i = result.size() - 1; i >= 0; --i) {
        int bits = 1;
        while ((1u << bits) < quint32(layout.options[i].choices.size()))
            ++bits;
        if (consumed + bits > 32)
            error("Layout choices exceed VIA 32-bit configuration");
        result[i] = packed & ((1u << bits) - 1);
        if (result[i] >= layout.options[i].choices.size())
            error("Invalid device layout option");
        packed >>= bits;
        consumed += bits;
    }
    return result;
}
quint32 configCrc(const QByteArray &b) {
    quint32 crc = 0xffffffff;
    for (auto ch : b) {
        crc ^= quint8(ch);
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return ~crc;
}
void validateBackup(const QByteArray &input, int rows, int cols) {
    const QByteArray b=decodeCalibrationRecord(input).canonical;
    if(rows<1||rows>255||cols<1||cols>255||rows*cols>4096||input.size()!=EC42_RECORD_BYTES(rows,cols)||b.size()!=9+rows*cols*2)
        error("EC42 v2.9 record dimensions mismatch");
}

QByteArray readBackup(DeviceClient &c, int rows, int cols) {
    QByteArray b;
    const int size = c.recordBytes();
    for (int offset = 0; offset < size; offset += 17) {
        int count = qMin(17, size - offset);
        QByteArray q;
        wire::put16(q, offset);
        q.append(char(count));
        auto p = c.request(wire::Op::Backup, q).payload;
        if (p.size() != count + 3 || p.left(3) != q)
            error("Invalid backup page");
        b += p.mid(3);
    }
    return b;
}
QByteArray readActive(DeviceClient &c,int rows,int cols){
    QByteArray b;
    for(int offset=0;offset<c.recordBytes();offset+=17){
        int count=qMin(17,c.recordBytes()-offset);QByteArray q;wire::put16(q,offset);q.append(char(count));
        auto p=c.request(wire::Op::ActiveRecord,q).payload;
        if(p.size()!=count+3||p.left(3)!=q)error("Invalid active calibration page");
        b+=p.mid(3);
    }
    validateBackup(b,rows,cols);
    if(wire::get16(b,9+2*rows*cols)!=c.recordMagic())error("Active EC record does not match descriptor");
    return b;
}
void restoreBackup(DeviceClient &c, const QByteArray &b, int rows, int cols) {
    if(!decodeCalibrationRecord(b).packed||!c.supportsVariableActuation())error("Only packed-format backups can be restored; no restore command sent");
    validateBackup(b, rows, cols);
    if((b.size()!=c.recordBytes()||wire::get16(b,9+2*rows*cols)!=c.recordMagic()))error("Restore record does not match connected descriptor; no write sent");
    QByteArray q;
    wire::put16(q, b.size());
    wire::put32(q, configCrc(b));
    auto begin = c.request(wire::Op::RestoreBegin, q).payload;
    if (begin.size() != 2 || !wire::get16(begin, 0))
        error("Invalid restore token");
    for (int off = 0; off < b.size(); off += 15) {
        auto chunk = b.mid(off, 15);
        QByteArray part = begin;
        wire::put16(part, off);
        part.append(char(chunk.size()));
        part += chunk;
        c.request(wire::Op::RestoreChunk, part);
    }
    c.request(wire::Op::RestoreCommit, begin);
    if (readBackup(c, rows, cols) != b)
        error("Restore readback mismatch: stop and inspect device");
}
CalibrationProgress calibrationProgress(const QVector<Sample> &samples, const QVector<int> &expected,
                                        bool saved) {
    CalibrationProgress p;
    for (int i : expected) {
        if (i < 0 || i >= samples.size())
            error("Calibration coordinate outside device");
        const auto &s = samples[i];
        if (!s.sampled) {
            ++p.unsampled;
            continue;
        }
        if (!s.captured) {
            ++p.incomplete;
            continue;
        }
        if (s.peak > 1023 || (saved && s.bottom != s.peak)) {
            ++p.invalid;
            continue;
        }
        ++p.complete;
    }
    p.verified = saved && p.complete > 0 && !p.invalid && !p.incomplete && !p.unsampled;
    return p;
}
SessionWorker::SessionWorker(std::unique_ptr<DeviceProvider> p, QObject *parent)
    : QThread(parent), provider(std::move(p)) {
    qRegisterMetaType<DeviceInfo>();
    qRegisterMetaType<QVector<DeviceInfo>>();
    qRegisterMetaType<QVector<Sample>>();
}
SessionWorker::~SessionWorker() {
    stop();
}
void SessionWorker::stop() {
    requestInterruption();
    wait();
}
void SessionWorker::selectDevice(QByteArray p) {
    QMutexLocker l(&mutex);
    desired = p;
}
void SessionWorker::selectCell(int index) {
    QMutexLocker l(&mutex);
    selectedCell = index;
}
void SessionWorker::command(quint64 generation, QString name, QByteArray data) {
    QMutexLocker l(&mutex);
    if (name == "cancel") {
        cancelCalibrationRequested = true;
        queue.clear(); // Cancel invalidates queued proposals/writes that have not begun.
        queue.prepend({generation, std::move(name), std::move(data)});
    } else {
        if (name == "start") cancelCalibrationRequested = false;
        queue.enqueue({generation, std::move(name), std::move(data)});
    }
}
HidProvider::~HidProvider() {
    if (initialized)
        hid_exit();
}
bool HidProvider::isVialCandidate(quint16 page, quint16 usage, const QString &serial) {
    return page == 0xFF60 && usage == 0x61 && serial.contains("vial:f64c2b3c");
}
QVector<DeviceInfo> HidProvider::enumerate() {
    if (!initialized) {
        if (hid_init())
            error("HIDAPI initialization failed");
        initialized = true;
    }
    QVector<DeviceInfo> devices;
    auto list = hid_enumerate(0, 0);
    for (auto d = list; d; d = d->next)
        if (isVialCandidate(d->usage_page, d->usage,
                            QString::fromWCharArray(d->serial_number ? d->serial_number : L""))) {
            DeviceInfo i;
            i.path = d->path;
            i.name = QString::fromWCharArray(d->product_string ? d->product_string : L"QMK Raw HID");
            i.serial = QString::fromWCharArray(d->serial_number ? d->serial_number : L"");
            i.vid = d->vendor_id;
            i.pid = d->product_id;
            devices << i;
        }
    hid_free_enumeration(list);
    return devices;
}
std::unique_ptr<Transport> HidProvider::open(const QByteArray &p) {
    return std::make_unique<HidTransport>(p);
}
void SessionWorker::run() {
    if (!provider)
        provider = std::make_unique<HidProvider>();
    std::unique_ptr<Transport> transport;
    std::unique_ptr<DeviceClient> client;
    DeviceInfo active;
    quint64 generation = 0;
    QElapsedTimer clock;
    clock.start();
    qint64 scanAt = -2000, pollAt = -1000, fullAt = -5000, selectedAt=0;
    QByteArray latestStatus;
    double lastDeviceTime = 0;
    bool haveDeviceTime = false;
    auto disconnect = [&](QString why) {
        cancelCalibrationRequested = false;latestStatus.clear();
        client.reset();
        transport.reset();
        active = {};
        {
            QMutexLocker l(&mutex);
            queue.clear();
        }
        emit connectionLost(why);
    };
    while (!isInterruptionRequested()) {
        try {
            if (clock.elapsed() - scanAt >= 2000) {
                scanAt = clock.elapsed();
                QVector<DeviceInfo> found;
                auto candidates = provider->enumerate();
                bool present = false;
                for (auto candidate : candidates) {
                    if (candidate.path == active.path && client) {
                        found << active;
                        present = true;
                        continue;
                    }
                    try {
                        auto probe = provider->open(candidate.path);
                        found << VialReader::identify(*probe, candidate);
                    } catch (const std::exception &e) {
                        emit message(candidate.name + ": " + e.what());
                    }
                }
                if (client && !present)
                    disconnect("Device disconnected; pending operations discarded");
                emit devicesChanged(found);
                QByteArray target;
                {
                    QMutexLocker l(&mutex);
                    target = desired;
                }
                if (!found.isEmpty()) {
                    DeviceInfo selected = found.first();
                    for (auto &i : found)
                        if (i.path == target)
                            selected = i;
                    if (!client || (!target.isEmpty() && target != active.path && selected.path == target)) {
                        if (client)
                            disconnect("Switching keyboard");
                        transport = provider->open(selected.path);
                        active = VialReader::identify(*transport, selected);
                        active.generation = ++generation;
                        client = std::make_unique<DeviceClient>(*transport);
                        client->hello();
                        if (!client->storageReady())
                            emit message("Keyboard connected, but firmware reports EEPROM storage NOT READY. Sensor inspection remains available; calibration saving cannot succeed. Do not reset calibration or bypass storage guards; inspect flash geometry/protection and firmware storage errors.");
                        try {client->setCalibrationRecord(readActive(*client,active.rows,active.cols));}
                        catch(const std::exception &e){emit message(QString("Saved calibration invalid: %1; Advanced restore may be needed").arg(e.what()));}
                        QByteArray definition;
                        quint32 options = 0;
                        try {
                            definition = VialReader::definition(*transport);
                            auto parsed = LayoutParser::parse(definition);
                            if (parsed.rows != active.rows || parsed.cols != active.cols)
                                error("Embedded layout/device dimension mismatch");

                        } catch (const std::exception &e) {
                            definition.clear();
                            emit message(QString("Layout fallback needed: %1").arg(e.what()));
                        }
                        try {
                            options = VialReader::options(*transport);
                        } catch (const std::exception &e) {
                            emit message(e.what());
                        }
                        haveDeviceTime = false;
                        emit identified(active, definition, options);
                        pollAt = -1000; fullAt = -5000;
                    }
                }
            }
            Command cmd;
            bool has = false;
            {
                QMutexLocker l(&mutex);
                if (!queue.isEmpty()) {
                    cmd = queue.dequeue();
                    has = true;
                }
            }
            if (has) {
                if (!client || cmd.generation != active.generation) {
                    emit message("Stale/disconnected command discarded");
                    continue;
                }
                try {
                    QByteArray result;
                    if (cmd.name != "read" && cmd.name != "backup") {
                        auto identity = VialReader::identify(*transport, active);
                        if (identity.uid != active.uid || identity.rows != active.rows ||
                            identity.cols != active.cols || identity.firmware != active.firmware)
                            error("Keyboard identity changed; reconnect before writing");
                    }
                    auto control=[&](int action,const QByteArray &tail=QByteArray{}){return client->request(wire::Op::CalibrationControl,QByteArray(1,char(action))+tail).payload;};
                    if(cmd.name=="bootloader"){
                        control(4); // discard only volatile calibration; never EEPROM
                        QByteArray q(32,'\0');q.replace(0,5,QByteArray::fromHex("ca45433431"));q[5]=7;
                        auto reply=transport->exchange(q);
                        if(reply.size()!=32||reply.left(8)!=q.left(8)||reply[8])error("Bootloader request not acknowledged");
                        emit completed(cmd.name,{});disconnect("Native bootloader requested; no firmware flashed. Application HID may disconnect.");scanAt=clock.elapsed();continue;
                    }
                    auto token=[&](){auto status=client->calibrationStatus();QByteArray q;wire::put16(q,wire::get16(status,1));return q;};
                    auto proposal=[&](const QByteArray &session){
                        QByteArray record;
                        for(int offset=0;offset<client->recordBytes();offset+=17){
                            int count=qMin(17,client->recordBytes()-offset);QByteArray q=session;
                            wire::put16(q,offset);q.append(char(count));auto p=client->request(wire::Op::CalibrationProposal,q).payload;
                            if(p.size()!=count+3||p.left(3)!=q.mid(2))error("Invalid calibration proposal page");
                            record+=p.mid(3);
                        }return record;
                    };
                    auto released=[&](){auto cells=client->poll();for(const auto &s:cells)if(s.sampled&&(s.active||s.debounced))error("Release all physical keys before this operation");};
                    auto commit=[&](const QByteArray &session){
                        if(cmd.name!="applyManual")released();
                        client->request(wire::Op::CalibrationValidate,session);
                        auto expected=proposal(session);validateBackup(expected,active.rows,active.cols);
                        if(cancelCalibrationRequested||isInterruptionRequested()){control(4);error("Calibration application cancelled before activation");}
                        client->request(wire::Op::CalibrationActivate,session);
                        auto activeHeader=client->request(wire::Op::Config,QByteArray(1,char(1))).payload;
                        if(activeHeader!=expected.left(9)){control(4);error("Active configuration readback mismatch; RAM activation cancelled");}
                        msleep(100); // Let matrix/debounce reconcile the newly active thresholds.
                        client->calibrationStatus();client->setCalibrationRecord(expected);
                        auto cells=client->poll();
                        for(int i=0;i<cells.size();++i){const auto &s=cells[i];if(!s.sampled)continue;int v=wire::get16(expected,9+2*i);
                            if(s.calibrated!=bool(v&0x8000)||s.savedQualified!=bool(v&0x4000)||(s.active||s.debounced)){control(4);error("Active partial calibration/disabled-key verification failed; not saved");}}
                        if(cancelCalibrationRequested||isInterruptionRequested()){control(4);error("Save cancelled; unsaved RAM calibration discarded");}
                        client->request(wire::Op::CalibrationSave,session);
                        auto actual=readBackup(*client,active.rows,active.cols);
                        if(decodeCalibrationRecord(actual).canonical!=decodeCalibrationRecord(expected).canonical || (decodeCalibrationRecord(actual).overrides!=decodeCalibrationRecord(expected).overrides))error("EEPROM readback mismatch; stop and restore backup; no automatic retry");
                        client->setCalibrationRecord(actual);return actual;
                    };
                    if(cmd.name=="vaRead"||cmd.name=="vaSet"||cmd.name=="vaRt"){
                        if(!client->supportsVariableActuation())error("Variable Actuation requires EC42 v2.9 firmware");
                        QByteArray index=cmd.name=="vaSet"?cmd.data.left(2):cmd.name=="vaRead"?cmd.data:QByteArray::fromHex("ffff");
                        if(index.size()!=2)error("Invalid actuation coordinate");
                        auto before=client->request(wire::Op::ActuationGet,index).payload;
                        if(before.size()!=20)error("Invalid actuation status");
                        if(cmd.name!="vaRead"){
                            QByteArray q;
                            auto op=wire::Op::ActuationSet;
                            q=cmd.data;q+=before.left(2);if(cmd.name=="vaRt")op=wire::Op::DynamicRtSet;
                            auto ack=client->request(op,q).payload;
                            result=client->request(wire::Op::ActuationGet,index).payload;
                            if(ack.size()!=2||result.size()!=20||ack!=result.left(2))error("Actuation RAM revision readback mismatch");
                            if(cmd.name=="vaSet"&&wire::get16(index,0)!=65535&&result[13]!=cmd.data[2])error("Override readback mismatch");
                            if(cmd.name=="vaSet"&&wire::get16(index,0)==65535&&wire::get16(result,6)!=(quint8(cmd.data[2])==99?1022:(quint8(cmd.data[2])*1023+50)/100))error("Global actuation readback mismatch");
                            if(cmd.name=="vaRt"&&result.mid(10,3)!=cmd.data)error("Dynamic RT readback mismatch");
                        }else result=before;
                    }else if(cmd.name=="start"){
                        auto status=client->calibrationStatus();
                        if(status[0]==1||status[0]==5)error("Cancel current calibration before starting another");
                        if(cancelCalibrationRequested)error("Cancelled before capture");
                        if(cmd.data.size()>1)error("Installed-key masks are unsupported; all sampled keys remain eligible");
                        control(1,cmd.data.isEmpty()?QByteArray(1,char(status[4])):cmd.data);result=client->calibrationStatus();
                    }else if(cmd.name=="finish"){
                        control(3);auto session=token();auto record=proposal(session);
                        auto cells=client->poll();auto savedProfile=decodeCalibrationRecord(readActive(*client,active.rows,active.cols));auto saved=savedProfile.canonical;QJsonArray keys;bool complete=false;
                        for(int i=0;i<cells.size();++i){
                            if(!cells[i].sampled){keys.append(QJsonObject{});continue;}
                            QByteArray q=session;wire::put16(q,i);auto e=client->request(wire::Op::CaptureEvidence,q).payload;
                            if(e.size()!=9||wire::get16(e,0)!=i)error("Invalid manual capture evidence; verify the matching EC42 firmware");
                            auto key=assessManualCapture(cells[i].baseline,wire::get16(e,2),wire::get16(e,4),quint8(e[6]),quint8(e[7]),wire::get16(record,1),wire::get16(record,3));
                            int state=quint8(e[8]);if(state>4)error("Invalid firmware capture state");
                            if(state==4)error("A sampled key cannot be excluded from partial calibration");
                            int v=wire::get16(record,9+2*i),old=wire::get16(saved,9+2*i);

                            if(state==1&&((v&0xc000)!=0xc000||(v&1023)!=wire::get16(e,4)))error("Manual peak/flags/evidence mismatch");
                            if(state==0&&v!=old)error("Untouched saved calibration changed unexpectedly");
                            if((state==2||state==3)&&v!=((old&~0x8000)|0x2000))error("Invalid capture was not safely disabled");
                            key["status"]=state;key["result"]=captureReason(state);key["valid"]=state==1;key["enabled"]=bool(v&0x8000);key["qualified"]=bool(v&0x4000);
                            key["preserved"]=state==0&&bool(v&0x8000);key["result"]=state==0?(v&0x8000?"Saved calibration retained; enabled":"Uncalibrated; disabled after Save"):state==1?"New valid capture; not yet saved":captureReason(state);
                            complete|=state==1;
                            if(!(v&0x8000)){key.remove("candidatePress");key.remove("candidateRelease");}
                            else {int p=savedProfile.overrides.value(i)?(savedProfile.overrides[i]==99?1022:(savedProfile.overrides[i]*1023+50)/100):wire::get16(record,1);int r=p-(wire::get16(record,1)-wire::get16(record,3));key["candidatePress"]=affineThreshold(p,cells[i].baseline,v&1023);key["candidateRelease"]=qMin(affineThreshold(r,cells[i].baseline,v&1023),(v&1023)-6);}
                            keys.append(key);
                        }
                        QJsonObject review{{"method",3},{"session",wire::get16(session,0)},{"record",QString::fromLatin1(record.toHex())},{"keys",keys},{"canApply",complete}};
                        if(complete){try{client->request(wire::Op::CalibrationValidate,session);}catch(const std::exception &e){review["canApply"]=false;review["validationError"]=QString::fromUtf8(e.what());}}
                        result=QJsonDocument(review).toJson(QJsonDocument::Compact);
                    }else if(cmd.name=="applyManual"){
                        auto review=QJsonDocument::fromJson(cmd.data).object();QByteArray session;wire::put16(session,review["session"].toInt());
                        if(session!=token())error("Stale calibration review; start a fresh session");
                        result=commit(session);
                    }else if(cmd.name=="resume"){
                        control(6);result=client->calibrationStatus();
                    }else if(cmd.name=="retry"){
                        if(cmd.data.size()!=2)error("Invalid retry coordinate");
                        control(5,cmd.data);result=cmd.data;
                    }else if(cmd.name=="cancel"||cmd.name=="reload"){
                        control(4);cancelCalibrationRequested=false;
                        client->setCalibrationRecord(readActive(*client,active.rows,active.cols));result=client->calibrationStatus();
                    }else if(cmd.name=="read"){
                        auto status=client->calibrationStatus();if(!status[5])error("Record/storage not ready; inspect console, power-cycle and restore a verified backup if required. No write was attempted.");
                        auto saved=readActive(*client,active.rows,active.cols);auto header=client->request(wire::Op::Config,QByteArray(1,char(1))).payload;
                        if(header!=decodeCalibrationRecord(saved).canonical.left(9))error("Active/saved settings differ; no derived record accepted");
                        client->setCalibrationRecord(saved);result=header;
                    }
                    else if(cmd.name=="backup")result=readBackup(*client,active.rows,active.cols);
                    else if(cmd.name=="restore"){
                        control(4);restoreBackup(*client,cmd.data,active.rows,active.cols);
                        client->setCalibrationRecord(cmd.data);result=cmd.data;
                    }else error("Unsupported application action");
                    emit completed(cmd.name, result);
                    pollAt = -1000; fullAt = -5000;
                } catch (const std::exception &e) {
                    // A failed save may leave EEPROM RAM cache different from active/physical data.
                    // Clear derived-record metadata; never present that cache as verified calibration.
                    client->setCalibrationRecord({});
                    emit message("Command failed; no automatic retry: " + QString::fromUtf8(e.what()));
                    emit completed("error", (cmd.name + ": " + QString::fromUtf8(e.what())).toUtf8());
                    pollAt = -1000; fullAt = -5000; // reconcile uncertain command outcome promptly
                }
            }
            if (client && clock.elapsed() - pollAt >= 1000) {
                pollAt = clock.elapsed();
                auto status = client->calibrationStatus();latestStatus=status;

                int selected;
                { QMutexLocker l(&mutex); selected = selectedCell; }
                bool full = clock.elapsed() - fullAt >= 5000 || status[0];
                auto samples = client->poll(selected, full);
                if (full) fullAt = clock.elapsed();
                pollAt = selectedAt = clock.elapsed(); // one second idle interval after the bounded snapshot

                if (!samples.isEmpty()) {
                    double now = samples[0].seconds;
                    if (haveDeviceTime && now < lastDeviceTime && lastDeviceTime - now < 2147483.648)
                        error("Firmware clock restarted; rediscovering without replaying writes");
                    lastDeviceTime = now;
                    haveDeviceTime = true;
                }
                emit frame(samples, status);
                if(client->supportsVariableActuation()){
                    QByteArray index;wire::put16(index,selected>=0&&selected<samples.size()&&samples[selected].sampled?selected:65535);
                    auto state=client->request(wire::Op::ActuationGet,index).payload;
                    if(state.size()!=20)error("Invalid Variable Actuation status");
                    emit actuationStatus(state);
                }
            }else if(client&&client->supportsVariableActuation()&&latestStatus.size()==6&&!latestStatus[0]&&clock.elapsed()-selectedAt>=200){
                int selected;{QMutexLocker l(&mutex);selected=selectedCell;}
                if(client->sampledCell(selected)){
                    auto samples=client->poll(selected,false,true);double now=samples[selected].seconds;
                    if(haveDeviceTime&&now<lastDeviceTime&&lastDeviceTime-now<2147483.648)error("Firmware clock restarted during selected-cell telemetry");
                    haveDeviceTime=true;lastDeviceTime=now;emit frame(samples,latestStatus);
                }
                selectedAt=clock.elapsed();
            }
        } catch (const std::exception &e) {
            disconnect(QString::fromUtf8(e.what()));
            scanAt = clock.elapsed();
        }
        msleep(20);
    }
    client.reset();
    transport.reset();
    provider.reset();
}
} // namespace ec
