// SPDX-License-Identifier: GPL-3.0-or-later
// Offline integration: every "device" here is a subprocess or fake provider.
#include "device.h"
#include "settings.h"
#include "calibration.h"
#include "mainwindow.h"
#include <QFile>
#include <QLabel>
#include <QDialog>
#include <QJsonDocument>
#include <QCheckBox>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QSlider>
#include <QComboBox>
#include <QtTest>
#include <atomic>
#include <lzma.h>
using namespace ec;
namespace {
QByteArray definition() {
    QFile f(QString(EC_FIXTURE_DIR) + "/class0413_ec.vial.json");
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error("fixture missing");
    return f.readAll();
}
QByteArray compress(const QByteArray &b) {
    QByteArray out(lzma_stream_buffer_bound(b.size()), '\0');
    size_t n = 0;
    auto rc = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, nullptr,
                                      reinterpret_cast<const uint8_t *>(b.constData()), b.size(),
                                      reinterpret_cast<uint8_t *>(out.data()), &n, out.size());
    if (rc != LZMA_OK)
        throw std::runtime_error("XZ encode failed");
    out.resize(n);
    return out;
}
struct State {
    std::atomic<bool> present{true};
    std::atomic<int> writes{0}, bootRequests{0};
    bool failDefinition = false, capture = false;
    std::atomic<bool> unsupported{false};
    std::atomic<int> now{-1};
    std::atomic<bool> advanceOnLatch{false};
    int count = 1;
    std::atomic<int> captureIndex{-1};
    int baseline = 100, captureBottom=700;
    bool stuck = false;
    std::atomic<bool> badReadback{false};
    std::atomic<bool> failStartOnce{false}, releaseOnLatch{false};
    bool holdAfterFinish = false;
    bool startRT = false;
};
class Peer : public Transport {
    QProcess process;
    QByteArray packed = compress(definition());
    std::shared_ptr<State> state;
    QByteArray call(const QByteArray &b) {
        if (process.write(b) != 32 || !process.waitForBytesWritten(2000))
            throw std::runtime_error("harness write failed");
        QByteArray reply;
        while (reply.size() < 32) {
            if (!process.bytesAvailable() && !process.waitForReadyRead(2000))
                throw std::runtime_error("harness reply failed");
            reply += process.read(32 - reply.size());
        }
        return reply;
    }

  public:
    explicit Peer(std::shared_ptr<State> s) : state(std::move(s)) {
        process.start(QString(EC_FWHARNESS));
        if (!process.waitForStarted())
            throw std::runtime_error("firmware harness missing");
        if (state->baseline != 100) {
            QByteArray q(32, '\0'); q.replace(0,4,"TEST"); q[4]=3;
            q[7]=char(state->baseline >> 8); q[8]=char(state->baseline); call(q);
        }

        if (state->stuck) {
            QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=1;q[7]=3;q[8]=32;call(q);
        }
    }
    ~Peer() override {
        process.closeWriteChannel();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }
    QByteArray exchange(const QByteArray &b) override {
        if (!state->present)
            throw std::runtime_error("fake unplug");
        QByteArray out(32, '\0');
        if (b[0] == 1) {
            out[0] = 1;
            out[2] = 9;
            return out;
        }
        if (quint8(b[0]) == 0xfe) {
            if (b[1] == 0) {
                out[0] = 6;
                out.replace(4, 8, QByteArray::fromHex("45464c4449414702"));
                return out;
            }
            if (b[1] == 1) {
                if (state->failDefinition)
                    return out;
                auto n = packed.size();
                for (int i = 0; i < 4; ++i)
                    out[i] = char(n >> (i * 8));
                return out;
            }
            if (b[1] == 2) {
                int page = quint8(b[2]) + (quint8(b[3]) << 8);
                auto part = packed.mid(page * 32, 32);
                out.replace(0, part.size(), part);
                return out;
            }
        }
        if (b.left(2) == QByteArray::fromHex("0202")) {
            out[0] = 2;
            out[1] = 2;
            out[5] = 3;
            return out;
        }
        if (b[0] == 7 || (b.mid(1,4)=="EC42" && (quint8(b[6])==0x25 || quint8(b[6])==0x19))) ++state->writes;
        if (b[0]==7 && b[2]==8 && b[3]==1 && state->failStartOnce.exchange(false)) {
            out=b;out[0]=char(0xff);return out;
        }
        if (b.mid(1, 4) == "EC42" && quint8(b[6]) == 0x11 && state->now >= 0) {
            QByteArray clock(32, '\0');
            clock.replace(0, 4, "TEST");
            int value = state->advanceOnLatch ? state->now.fetch_add(250) : state->now.load();
            for (int i = 0; i < 4; ++i)
                clock[8 + i] = char(value >> (24 - i * 8));
            call(clock);
        }
        auto reply = call(b);
        if(b.left(6)==QByteArray::fromHex("ca4543343107")&&!reply[8]){++state->bootRequests;state->present=false;}
        // Model physical release after an operation; do not reset capture/EEPROM to make tests pass.
        if ((b[0]==7 && b[2]==8 && b[3]==0 && reply[0]==7 && !state->holdAfterFinish) ||
            (b.mid(1,4)=="EC42" && quint8(b[6])==0x11 && state->releaseOnLatch.exchange(false)) ||
            (b.mid(1,4)=="EC42" && quint8(b[6])==0x1b && !reply[9])) {
            QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=5;
            q[7]=char(state->baseline>>8);q[8]=char(state->baseline);call(q);
        }
        if (state->badReadback && b.mid(1,4)=="EC42" && quint8(b[6])==0x15) {
            reply[13]=2; reply[14]=char(88); // valid but unexpected APC=600
        }
        if (state->unsupported && b.mid(1, 4) == "EC42" && quint8(b[6]) == 0x10) {
            reply[24] = 0;
            reply[25] = 6;
        }
        if (state->capture && b.mid(1, 4) == "EC42" && quint8(b[6]) == 0x14 && reply[12]==1) {
            // Model sustained released/pressed plateaus. The original mock
            // started pressing before even one full resting block was scanned.
            auto hold=[&](int value){for(int scan=0;scan<32;++scan){QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=5;q[7]=char(value>>8);q[8]=char(value);call(q);}};
            hold(state->baseline);
            for (int i = 0; i < 105; ++i) {
                if(state->captureIndex>=0&&i!=state->captureIndex)continue;
                QByteArray command(32, '\0');
                command.replace(0, 4, "TEST");
                command[4] = 1;
                command[5] = char(i >> 8);
                command[6] = char(i);
                command[7] = char(state->captureBottom>>8);
                command[8] = char(state->captureBottom);
                call(command);
            }
            for(int n=0;n<32;++n){QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=2;call(q);}
            hold(state->baseline);
        }
        if(b.mid(1,4)=="EC42"&&quint8(b[6])==0x20&&(b[12]==3||b[12]==4)){QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=5;q[8]=100;call(q);}
        return reply;
    }
};
class Provider : public DeviceProvider {
    std::shared_ptr<State> state;

  public:
    explicit Provider(std::shared_ptr<State> s) : state(std::move(s)) {}
    QVector<DeviceInfo> enumerate() override {
        if (!state->present)
            return {};
        DeviceInfo a;
        a.path = "fake-1";
        a.name = "Offline firmware fixture";
        a.vid = 0x2024;
        a.pid = 0x0319;
        a.serial = "unit-test";
        if (state->count == 2) {
            auto b = a;
            b.path = "fake-2";
            b.serial = "second-unit";
            return {a, b};
        }
        return {a};
    }
    std::unique_ptr<Transport> open(const QByteArray &) override {
        return std::make_unique<Peer>(state);
    }
};
} // namespace
class LiveTests:public QObject {
Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QCoreApplication::setOrganizationName("qmk-ec-util-tests");
    }
    void discoveryCandidateFilter() {
        QVERIFY(HidProvider::isVialCandidate(0xff60, 0x61, "vial:f64c2b3c"));
        QVERIFY(!HidProvider::isVialCandidate(0xff60, 0x61, "unrelated raw HID"));
        QVERIFY(!HidProvider::isVialCandidate(0xff60, 1, "vial:f64c2b3c"));
    }
    void actualFirmwareCodec() {
        auto state = std::make_shared<State>();
        Peer peer(state);
        DeviceInfo info;
        info = VialReader::identify(peer, info);
        QCOMPARE(info.rows, 5);
        QCOMPARE(info.cols, 21);
        QCOMPARE(info.uid.toHex(), QByteArray("45464c4449414702"));
        DeviceClient client(peer);
        client.hello();
        QVERIFY(!client.simulated());
        auto samples = client.poll();
        QCOMPARE(samples.size(), 105);
        QVERIFY(!samples[37].sampled);
        QCOMPARE(samples[0].raw, quint16(100));
        auto backup = readBackup(client, 5, 21);
        QCOMPARE(backup.size(), 332);
        QVERIFY_EXCEPTION_THROWN(restoreBackup(client, backup, 5, 21),std::runtime_error);
        QCOMPARE(readBackup(client, 5, 21), backup);
        auto stateRam=readActive(client,5,21);validateBackup(stateRam,5,21);
        for(const auto &cell:client.poll())QVERIFY(!cell.calibrated&&!cell.active);
    }
    void selectedTelemetryIsBounded(){
        auto state=std::make_shared<State>();Peer peer(state);DeviceClient client(peer);client.hello();client.calibrationStatus();
        auto before=client.poll();QCOMPARE(before.size(),105);
        QByteArray q(32,0);q.replace(0,4,"TEST");q[4]=1;q[6]=1;q[7]=2;q[8]=char(188);peer.exchange(q);
        auto selected=client.poll(1,false,true);QCOMPARE(selected[1].raw,700);QCOMPARE(selected[0].raw,before[0].raw);QCOMPARE(selected[0].seconds,before[0].seconds);
        QVERIFY_EXCEPTION_THROWN(client.poll(37,false,true),std::runtime_error);
    }
    void currentRecordIntegrity(){
        auto state=std::make_shared<State>();state->capture=true;state->captureIndex=0;Peer peer(state);DeviceClient client(peer);client.hello();
        auto token=client.request(wire::Op::CalibrationControl,QByteArray(1,1)).payload;
        client.calibrationStatus(); // peer models sustained released/pressed/released blocks
        client.request(wire::Op::CalibrationControl,QByteArray(1,3));
        client.request(wire::Op::CalibrationValidate,token);client.request(wire::Op::CalibrationActivate,token);client.request(wire::Op::CalibrationSave,token);
        auto old=readBackup(client,5,21);auto before=client.request(wire::Op::ActuationGet,QByteArray::fromHex("ffff")).payload;
        auto ack=before.left(2);
        QByteArray q=QByteArray::fromHex("000014")+ack;client.request(wire::Op::ActuationSet,q);
        QByteArray clock(32,0);clock.replace(0,4,"TEST");clock[4]=0;clock[10]=char(0x0b);clock[11]=char(0xb8);peer.exchange(clock);peer.exchange(clock);
        auto raw=readBackup(client,5,21);QVERIFY(raw!=old);auto decoded=decodeCalibrationRecord(raw);
        QVERIFY(decoded.packed);QCOMPARE(decoded.canonical.left(9),decodeCalibrationRecord(old).canonical.left(9));QCOMPARE(decoded.overrides[0],20);
        const QSet<int> absent={37,58,62,78,83,89,90,91,93,94,95,96,100,101};
        for(int i=0;i<105;i++)if(!absent.contains(i))QCOMPARE(decoded.canonical.mid(9+2*i,2),decodeCalibrationRecord(old).canonical.mid(9+2*i,2));
        validateBackup(raw,5,21);
        for(int i=0;i<raw.size();++i)for(int bit=0;bit<8;++bit){auto corrupt=raw;corrupt[i]=char(quint8(corrupt[i])^(1<<bit));QVERIFY_EXCEPTION_THROWN(validateBackup(corrupt,5,21),std::runtime_error);}
        client.setCalibrationRecord(raw);client.calibrationStatus();auto samples=client.poll();QCOMPARE(samples[0].fixedPress,220);QCOMPARE(samples[0].fixedRelease,190);
        // Corrupt only mocked persisted CRC; recovery must remain available without reconnect.
        QByteArray corrupt(32,0);corrupt.replace(0,4,"TEST");corrupt[4]=15;corrupt[5]=0;corrupt[6]=char(221);corrupt[8]=char(quint8(raw[222])^1);peer.exchange(corrupt);
        client.setCalibrationRecord({});auto invalid=client.calibrationStatus();QVERIFY(invalid[5]);QCOMPARE(quint8(invalid[3]),3);
        QCOMPARE(quint8(client.request(wire::Op::ActuationGet,QByteArray::fromHex("ffff")).payload[4]),4);
        auto disabled=client.poll();for(const auto &cell:disabled)QVERIFY(!cell.calibrated&&!cell.active);
        restoreBackup(client,old,5,21);QCOMPARE(readBackup(client,5,21),old);
        QCOMPARE(client.request(wire::Op::ActuationGet,QByteArray::fromHex("ffff")).payload[5],char(1));
    }
    void embeddedLayout() {
        auto state = std::make_shared<State>();
        Peer peer(state);
        QCOMPARE(VialReader::definition(peer), definition());
        auto l = LayoutParser::parse(VialReader::definition(peer));
        auto options = VialReader::unpackOptions(l, VialReader::options(peer));
        QCOMPARE(options, QVector<int>({0, 0, 0, 1, 1}));
        QVERIFY(!l.visible(options).isEmpty());
        QCOMPARE(state->writes.load(), 0);
    }
    void rejectCompressed() {
        QVERIFY_EXCEPTION_THROWN(VialReader::decompress("broken"), std::runtime_error);
        auto packed = compress(definition());
        packed[packed.size() / 2] ^= 1;
        QVERIFY_EXCEPTION_THROWN(VialReader::decompress(packed), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(VialReader::decompress(QByteArray(256 * 1024 + 1, 'x')), std::runtime_error);
    }
    void layoutFallback() {
        auto s = std::make_shared<State>();
        s->failDefinition = true;
        Peer peer(s);
        VialReader::identify(peer, {});
        QVERIFY_EXCEPTION_THROWN(VialReader::definition(peer), std::runtime_error);
        QCOMPARE(s->writes.load(), 0);
    }
    void unsupportedDeviceNoWrites() {
        auto s = std::make_shared<State>();
        s->unsupported = true;
        Peer peer(s);
        QVERIFY_EXCEPTION_THROWN(VialReader::identify(peer, {}), std::runtime_error);
        QCOMPARE(s->writes.load(), 0);
    }
    void malformedVial() {
        class Broken : public Transport {
            QByteArray exchange(const QByteArray &) override {
                return QByteArray(31, '\0');
            }
        } bad;
        QVERIFY_EXCEPTION_THROWN(VialReader::identify(bad, {}), std::runtime_error);
    }
    void failedTransportAndReconnect() {
        struct Timeout : Transport {
            QByteArray exchange(const QByteArray &) override {throw std::runtime_error("HID timeout");}
        } t;
        DeviceClient c(t);
        QVERIFY_EXCEPTION_THROWN(c.hello(),std::runtime_error);
    }
    void allMatrixCoordinates() {
        auto s=std::make_shared<State>();Peer peer(s);DeviceClient client(peer);client.hello();
        for(int i=0;i<105;i++) {
            QByteArray q(32,'\0');q.replace(0,4,"TEST");q[4]=1;q[5]=char(i>>8);q[6]=char(i);
            q[7]=char((100+i)>>8);q[8]=char(100+i);peer.exchange(q);
        }
        auto values=client.poll();int sampled=0;
        for(int i=0;i<values.size();i++) {
            if(values[i].sampled) {++sampled;QCOMPARE(values[i].raw,quint16(100+i));}
            else QCOMPARE(values[i].raw,quint16(0));
        }
        QCOMPARE(sampled,91);
    }
    void backupValidation() {
        QCOMPARE(configCrc("123456789"), quint32(0xcbf43926));
        QVERIFY_EXCEPTION_THROWN(validateBackup({}, 5, 21), std::runtime_error);
        auto s = std::make_shared<State>();
        Peer peer(s);
        DeviceClient client(peer);
        client.hello();
        auto b = readBackup(client, 5, 21);
        b[0] = 2;
        QVERIFY_EXCEPTION_THROWN(restoreBackup(client, b, 5, 21), std::runtime_error);
        QCOMPARE(s->writes.load(), 0);
    }
    void discoveryDisconnectReconnect() {
        auto s = std::make_shared<State>();
        SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy connected(&worker, &SessionWorker::identified), frames(&worker, &SessionWorker::frame),
            lost(&worker, &SessionWorker::connectionLost);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(connected.size() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() >= 1, 5000);
        QCOMPARE(s->writes.load(), 0);
        auto first = qvariant_cast<DeviceInfo>(connected[0][0]);
        s->present = false;
        QTRY_VERIFY_WITH_TIMEOUT(lost.size() >= 1, 4000);
        worker.command(first.generation, "start");
        s->present = true;
        QTRY_VERIFY_WITH_TIMEOUT(connected.size() >= 2, 6000);
        auto second = qvariant_cast<DeviceInfo>(connected[1][0]);
        QVERIFY(second.generation != first.generation);
        QTest::qWait(100);
        QCOMPARE(s->writes.load(), 0);
        worker.stop();
    }
    void resetClockRediscovery() {
        auto s = std::make_shared<State>();
        s->now = 10000;
        SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy ready(&worker, &SessionWorker::identified), frames(&worker, &SessionWorker::frame),
            lost(&worker, &SessionWorker::connectionLost);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() >= 1, 5000);
        s->now = 1;
        QTRY_VERIFY_WITH_TIMEOUT(lost.size() >= 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() >= 2, 5000);
        QCOMPARE(s->writes.load(), 0);
        worker.stop();
    }
    void revalidateBeforeWrite() {
        auto s = std::make_shared<State>();
        SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy ready(&worker, &SessionWorker::identified), done(&worker, &SessionWorker::completed);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() >= 1, 5000);
        auto info = qvariant_cast<DeviceInfo>(ready[0][0]);
        s->unsupported = true;
        worker.command(info.generation, "start");
        QTRY_VERIFY_WITH_TIMEOUT(done.size() >= 1, 4000);
        QCOMPARE(done[0][0].toString(), QString("error"));
        QCOMPARE(s->writes.load(), 0);
        worker.stop();
    }
    void multipleDeviceSelection() {
        auto s = std::make_shared<State>();
        s->count = 2;
        SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy list(&worker, &SessionWorker::devicesChanged), ready(&worker, &SessionWorker::identified);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() == 1, 5000);
        QCOMPARE(qvariant_cast<QVector<DeviceInfo>>(list[0][0]).size(), 2);
        worker.selectDevice("fake-2");
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() == 2, 5000);
        QCOMPARE(qvariant_cast<DeviceInfo>(ready[1][0]).path, QByteArray("fake-2"));
        QCOMPARE(s->writes.load(), 0);
        worker.stop();
    }
    void liveGuiWithFakeProvider() {
        auto s = std::make_shared<State>();
        MainWindow w(nullptr, true, std::make_unique<Provider>(s));
        QTRY_VERIFY_WITH_TIMEOUT(w.healthy(), 5000);
        QCOMPARE(w.matrixCount(), 105);
        QCOMPARE(w.currentLayout().name, QString("MM Studio-0413 (Cipulot EC port)"));
        QCOMPARE(s->writes.load(), 0);
    }
    void repeatedExplicitWorkerSessions(){
        auto s=std::make_shared<State>();s->capture=true;
        SessionWorker worker(std::make_unique<Provider>(s));QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed);
        worker.start();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),5000);auto info=qvariant_cast<DeviceInfo>(ready[0][0]);
        for(int i=0;i<3;++i){done.clear();worker.command(info.generation,"start");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QVERIFY2(done[0][0].toString()=="start",done[0][1].toByteArray().constData());
            done.clear();worker.command(info.generation,i==1?"cancel":"finish");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),i==1?QString("cancel"):QString("finish"));}
        QCOMPARE(s->writes.load(),0);worker.stop();
    }
    void manualReviewThenExplicitSave(){
        auto s=std::make_shared<State>();s->capture=true;
        SessionWorker worker(std::make_unique<Provider>(s));QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed);
        worker.start();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),5000);auto info=qvariant_cast<DeviceInfo>(ready[0][0]);
        worker.command(info.generation,"start");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);done.clear();worker.command(info.generation,"finish");
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QVERIFY2(done[0][0].toString()=="finish",done[0][1].toByteArray().constData());auto review=done[0][1].toByteArray();QCOMPARE(s->writes.load(),0);
        done.clear();worker.command(info.generation,"applyManual",review);QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),QString("applyManual"));QCOMPARE(s->writes.load(),1);worker.stop();
    }
    void failedActiveReadbackNeverSaves(){
        auto s=std::make_shared<State>();s->capture=true;SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed);worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),5000);auto info=qvariant_cast<DeviceInfo>(ready[0][0]);worker.command(info.generation,"start");
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);done.clear();worker.command(info.generation,"finish");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);
        auto review=done[0][1].toByteArray();done.clear();s->badReadback=true;worker.command(info.generation,"applyManual",review);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),QString("error"));
        QVERIFY(done[0][1].toByteArray().contains("readback mismatch"));QCOMPARE(s->writes.load(),0);worker.stop();
    }
    void independentBootloaderRecovery(){
        auto s=std::make_shared<State>();SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed);worker.start();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),5000);
        auto info=qvariant_cast<DeviceInfo>(ready[0][0]);worker.command(info.generation,"bootloader");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);
        QCOMPARE(done[0][0].toString(),QString("bootloader"));QCOMPARE(s->bootRequests.load(),1);QCOMPARE(s->writes.load(),0);worker.stop();
    }
    void incrementalPartialProtocolAndReadback(){
        auto s=std::make_shared<State>();s->capture=true;s->captureIndex=0;SessionWorker worker(std::make_unique<Provider>(s));
        QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed);worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),5000);auto info=qvariant_cast<DeviceInfo>(ready[0][0]);QByteArray previous;
        for(int index:{0,1}){
            s->captureIndex=index;done.clear();worker.command(info.generation,"start");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),QString("start"));
            done.clear();worker.command(info.generation,"finish");QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),QString("finish"));
            auto data=done[0][1].toByteArray();auto review=QJsonDocument::fromJson(data).object();QVERIFY(review["canApply"].toBool());auto record=QByteArray::fromHex(review["record"].toString().toLatin1());
            QCOMPARE(wire::get16(record,9+2*index)&0xc000,0xc000);QCOMPARE(wire::get16(record,9+4),0);
            if(index==1)QCOMPARE(record.mid(9,2),previous.mid(9,2));
            done.clear();worker.command(info.generation,"applyManual",data);QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(),5000);QCOMPARE(done[0][0].toString(),QString("applyManual"));QCOMPARE(decodeCalibrationRecord(done[0][1].toByteArray()).canonical,decodeCalibrationRecord(record).canonical);previous=record;
        }
        QCOMPARE(s->writes.load(),2);worker.stop();
    }
    void guiManualAndSliderSynchronization(){
        auto s=std::make_shared<State>();s->capture=true;s->captureIndex=0;s->captureBottom=200; // minimum qualified span; legacy RT70 cannot fit, GUI must send bounded slider value
        MainWindow w(nullptr,true,std::make_unique<Provider>(s));
        auto button=[&](const QString &name){for(auto b:w.findChildren<QPushButton*>())if(b->text()==name)return b;return static_cast<QPushButton*>(nullptr);};
        QTRY_VERIFY_WITH_TIMEOUT(w.healthy()&&button("Start Manual Calibration")->isEnabled(),5000);
        bool reviewed=false;QTimer answer;answer.setInterval(20);
        connect(&answer,&QTimer::timeout,&answer,[&](){
            if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();
            else if(auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){
                if(dialog->windowTitle().startsWith("Calibration review")){auto table=dialog->findChild<QTableWidget*>();QVERIFY(table);QCOMPARE(table->columnCount(),10);reviewed=true;dialog->reject();}
            }
        });answer.start();button("Start Manual Calibration")->click();
        QTRY_VERIFY_WITH_TIMEOUT(button("Finish Calibration")->isEnabled(),5000);button("Finish Calibration")->click();
        QTRY_VERIFY_WITH_TIMEOUT(reviewed&&button("Apply & Save")->isEnabled(),5000);QCOMPARE(s->writes.load(),0);
        button("Apply & Save")->click();QTRY_COMPARE_WITH_TIMEOUT(s->writes.load(),1,5000);
        QVERIFY(!button("Migrate saved calibration format…"));
        auto global=w.findChild<QSlider*>("globalActuation");auto per=w.findChild<QSlider*>("keyActuation");
        auto dynamic=w.findChild<QCheckBox*>("rtEnabled");auto slider=w.findChild<QSlider*>("rtSensitivity");
        QVERIFY(global&&per&&dynamic&&slider);
        QTRY_COMPARE_WITH_TIMEOUT(global->minimum(),6,5000);QCOMPARE(global->maximum(),99);
        QTRY_VERIFY_WITH_TIMEOUT(global->isEnabled(),5000);
        global->setValue(65);
        auto effective=w.findChild<QLabel*>("rtEffectiveStatus");QVERIFY(effective);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("revision 1"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(per->isEnabled(),5000);per->setValue(60);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("revision 2"),5000);
        dynamic->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("Dynamic RT ON"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(slider->isEnabled(),5000);
        slider->setValue(slider->maximum());
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("revision 4"),5000);
        auto press=w.findChild<QSpinBox*>("rtPress"),release=w.findChild<QSpinBox*>("rtRelease");QVERIFY(press&&release);
        QTRY_VERIFY_WITH_TIMEOUT(press->isEnabled(),5000);press->setValue(6);release->setValue(7);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("6/7 ADC counts"),5000);
        auto status=w.findChild<QLabel*>("settingsStatus");QVERIFY(status);
        QVERIFY(status->text().contains("Applied")||status->text().contains("Saving")||status->text().contains("Saved"));
        QCOMPARE(s->writes.load(),1); // Manual save only; automatic physical writes are counted by separate scanner tests.
        // Deepest endpoint with a100-count span previously produced an inverted RT range.
        per->setValue(99);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("4/4 ADC counts"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(dynamic->isEnabled(),5000);
        dynamic->setChecked(false);QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("Dynamic RT OFF"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(dynamic->isEnabled(),5000);dynamic->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(effective->text().contains("Dynamic RT ON"),5000);
        QVERIFY(effective->text().contains("4/4 ADC counts"));

    }
    void noInstalledKeyEditingOrImplicitDisable(){
        auto s=std::make_shared<State>();MainWindow w(nullptr,true,std::make_unique<Provider>(s));
        QTRY_VERIFY_WITH_TIMEOUT(w.healthy()&&w.installedCount()==91,5000);
        auto summary=w.findChild<QLabel*>("calibrationProgressSummary");QVERIFY(summary);
        QTRY_VERIFY_WITH_TIMEOUT(summary->text().contains("Calibration required"),5000);
        for(auto b:w.findChildren<QPushButton*>())QVERIFY(!b->text().contains("Migrate"));
        QVERIFY(!w.findChild<QCheckBox*>("editInstalledKeys"));
        w.selectKey(0,0);QCOMPARE(w.installedCount(),91);QVERIFY(w.keyInstalled(0,0));
        QVERIFY(!w.keyInstalled(1,16));QCOMPARE(s->writes.load(),0);
    }
    void guiWorkflowAndWarning(){
        auto s=std::make_shared<State>();MainWindow w(nullptr,true,std::make_unique<Provider>(s));QTRY_VERIFY_WITH_TIMEOUT(w.healthy(),5000);
        auto warning=w.findChild<QLabel*>("vialWarning");QVERIFY(warning);QCOMPARE(warning->text(),QString("Please close Vial before using QMK EC Utility."));
        for(auto title:{"Start Manual Calibration","Finish Calibration","Cancel Calibration","Review Results","Apply & Save"}){
            bool found=false;for(auto b:w.findChildren<QPushButton*>())found|=b->text()==title;QVERIFY(found);}
        QVERIFY(!w.findChild<QLabel*>("calibrationCountdown"));QVERIFY(w.findChild<QSlider*>("rtSensitivity"));QCOMPARE(s->writes.load(),0);
    }

};
QTEST_MAIN(LiveTests)
#include "test_live.moc"
