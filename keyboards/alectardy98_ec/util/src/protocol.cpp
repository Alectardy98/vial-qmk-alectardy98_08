// SPDX-License-Identifier: GPL-3.0-or-later
#include "protocol.h"
#include "boardprofile.h"
#include "settings.h"
#include <QElapsedTimer>
#include <stdexcept>
namespace ec {
namespace wire {
void put16(QByteArray &b, quint16 n) {
    b.append(char(n >> 8));
    b.append(char(n));
}
quint16 get16(const QByteArray &b, int o) {
    if (o < 0 || o + 2 > b.size())
        throw std::runtime_error("Truncated u16");
    return quint16(quint8(b[o])) << 8 | quint8(b[o + 1]);
}
void put32(QByteArray &b, quint32 n) {
    put16(b, n >> 16);
    put16(b, n);
}
quint32 get32(const QByteArray &b, int o) {
    return quint32(get16(b, o)) << 16 | get16(b, o + 2);
}
QByteArray encode(const Packet &p) {
    if (p.payload.size() > 20)
        throw std::runtime_error("EC42 payload exceeds 20 bytes");
    quint8 op = quint8(p.op);
    if (op < 0x10 || (op > 0x19 && op != 0x1B && op != 0x1C && op != 0x1D && ((op < 0x20 || op > 0x2b))))
        throw std::runtime_error("Unknown EC42 opcode");
    if (quint8(p.status) > 4 || (!p.response && p.status != Status::Ok))
        throw std::runtime_error("Invalid status");
    QByteArray b;
    b.append(char(0xCA));
    b.append("EC42", 4);
    b.append(char(Version));
    b.append(char(op));
    put16(b, p.sequence);
    b.append(char(p.status));
    b.append(char(p.payload.size()));
    b.append(char(p.response ? 1 : 0));
    b += p.payload;
    b.resize(32, char(0));
    return b;
}
Packet decode(const QByteArray &b) {
    if (b.size() != 32)
        throw std::runtime_error("EC42 must contain exactly 32 bytes");
    if (quint8(b[0]) != 0xCA || b.mid(1, 4) != "EC42")
        throw std::runtime_error("Wrong EC42 magic");
    if (quint8(b[5]) != Version)
        throw std::runtime_error("Unsupported EC42 major");
    quint8 op = quint8(b[6]), len = quint8(b[10]), flags = quint8(b[11]), status = quint8(b[9]);
    if (op < 0x10 || (op > 0x19 && op != 0x1B && op != 0x1C && op != 0x1D && ((op < 0x20 || op > 0x2b))) || len > 20 || flags > 1 || status > 4 ||
        (!flags && status))
        throw std::runtime_error("Invalid EC42 header");
    for (int i = 12 + len; i < 32; ++i)
        if (b[i] != 0)
            throw std::runtime_error("Nonzero EC42 padding");
    return Packet{Op(op), get16(b, 7), Status(status), flags == 1, b.mid(12, len)};
}
} // namespace wire
MockTransport::MockTransport(int rows, int cols, const QVector<bool> &mask) : simulator(rows, cols, mask) {
    simulator.step(0);
}
QByteArray MockTransport::exchange(const QByteArray &report) {
    auto req = wire::decode(report);
    if (req.response)
        throw std::runtime_error("Mock expects a request");
    wire::Packet res{req.op, req.sequence, wire::Status::Ok, true, {}};
    switch (req.op) {
    case wire::Op::Hello:
        if (!req.payload.isEmpty()) {
            res.status = wire::Status::Malformed;
            break;
        }
        res.payload = "QEC2";
        res.payload.append(char(EC42_MINOR)); // Current protocol, read-only simulator
        wire::put16(res.payload, simulator.rows());
        wire::put16(res.payload, simulator.cols());
        wire::put16(res.payload, 1023);
        res.payload.append(char(1));      // increasing polarity
        wire::put16(res.payload, 0x000F); // simulation, snapshots, cell metadata, optional sampling
        res.payload.append(char(5));      // max page samples
        res.payload.append(char(0));      // no persistent writes
        break;
    case wire::Op::Descriptor:
        wire::put16(res.payload,0xa105);wire::put16(res.payload,EC42_RECORD_BYTES(simulator.rows(),simulator.cols()));
        wire::put16(res.payload,1023);wire::put16(res.payload,2);wire::put32(res.payload,0x8000);wire::put32(res.payload,0xc000);
        wire::put16(res.payload,128);res.payload.append(char(0));res.payload.append(char(EC42_STORAGE_FORMAT));break;
    case wire::Op::Latch:
        if (!req.payload.isEmpty()) {
            res.status = wire::Status::Malformed;
            break;
        }
        snapshot = simulator.samples();
        if (++snapshotId == 0)
            ++snapshotId;
        millis = quint32(snapshot[0].seconds * 1000);
        wire::put16(res.payload, snapshotId);
        wire::put32(res.payload, millis);
        wire::put16(res.payload, snapshot.size());
        break;
    case wire::Op::ReadPage: {
        if (req.payload.size() != 5) {
            res.status = wire::Status::Malformed;
            break;
        }
        quint16 id = wire::get16(req.payload, 0), start = wire::get16(req.payload, 2);
        quint8 count = quint8(req.payload[4]);
        if (!snapshotId || id != snapshotId) {
            res.status = wire::Status::Stale;
            break;
        }
        if (!count || count > 5 || start + count > snapshot.size()) {
            res.status = wire::Status::Range;
            break;
        }
        wire::put16(res.payload, id);
        wire::put16(res.payload, start);
        res.payload.append(char(count));
        for (int i = start; i < start + count; ++i) {
            const auto &s = snapshot[i];
            wire::put16(res.payload, s.raw);
            res.payload.append(char((s.sampled ? 1 : 0) | (s.active ? 2 : 0) | (s.debounced ? 4 : 0) |
                                    (s.calibrated ? 8 : 0)));
        }
        break;
    }
    case wire::Op::ReadCell: {
        if (req.payload.size() != 4) {
            res.status = wire::Status::Malformed;
            break;
        }
        auto id = wire::get16(req.payload, 0), index = wire::get16(req.payload, 2);
        if (!snapshotId || id != snapshotId) {
            res.status = wire::Status::Stale;
            break;
        }
        if (index >= snapshot.size()) {
            res.status = wire::Status::Range;
            break;
        }
        const auto &s = snapshot[index];
        wire::put16(res.payload, id);
        wire::put16(res.payload, index);
        for (auto v : {s.baseline, s.bottom, s.press, s.release, s.minimum, s.maximum, s.peak, s.extremum})
            wire::put16(res.payload, v);
        break;
    }
    default:
        res.status = wire::Status::Unsupported;
        break;
    }
    return wire::encode(res);
}
wire::Packet DeviceClient::request(wire::Op op, const QByteArray &payload) {
    if(quint8(op)>=0x20&&op!=wire::Op::Descriptor&&!supportsExplicitCalibration())
        throw std::runtime_error("Calibration requires an explicit EC42 calibration capability; no command sent");
    if(quint8(op)>=0x28&&op!=wire::Op::Descriptor&&!supportsVariableActuation())throw std::runtime_error("Variable Actuation requires EC42 v2.9; no command sent");
    wire::Packet req{op, ++sequence, wire::Status::Ok, false, payload};
    auto res = wire::decode(transport.exchange(wire::encode(req)));
    if (!res.response || res.sequence != req.sequence || res.op != op)
        throw std::runtime_error("Mismatched EC42 reply");
    if (res.status != wire::Status::Ok)
        throw std::runtime_error("EC42 operation rejected (status " + std::to_string(int(res.status)) +
                                 "): malformed/state, unsupported, range/baseline mismatch, or stale "
                                 "snapshot/token; inspect before retrying");
    return res;
}
QPair<int, int> DeviceClient::hello() {
    auto p = request(wire::Op::Hello).payload;
    if(p.size()!=16||p.left(4)!="QEC2"||quint8(p[4])!=EC42_MINOR||wire::get16(p,9)!=1023||
       quint8(p[11])!=1||(wire::get16(p,12)&6)!=6||quint8(p[14])!=5||quint8(p[15])>1)
        throw std::runtime_error("EC42 v2.9 required; unsupported capability response, no write sent");
    minor = quint8(p[4]);
    capabilities = wire::get16(p, 12);
    rows = wire::get16(p, 5);
    cols = wire::get16(p, 7);
    if (!rows || !cols || rows > 255 || cols > 255 || rows * cols > 4096)
        throw std::runtime_error("Invalid device dimensions");
    storageAvailable = true; storageMagic=0;
    {
        auto d=request(wire::Op::Descriptor).payload;
        const int magic=d.size()==20?wire::get16(d,0):0;
        if(!magic||magic==65535||d.size()!=20||wire::get16(d,0)!=magic||wire::get16(d,2)!=recordBytes()||wire::get16(d,4)!=1023||wire::get16(d,6)>=32||wire::get32(d,12)<=wire::get32(d,8)||wire::get32(d,12)>quint32(wire::get16(d,16))*1024u||quint8(d[18])>1||d[19]!=1)
            throw std::runtime_error("Unsupported EC storage/sensor descriptor; no write sent");
        storageAvailable = quint8(d[18]) == 1; storageMagic=magic;
    }
    return {rows, cols};
}
QByteArray DeviceClient::calibrationStatus() {
    if(!rows||minor!=EC42_MINOR)throw std::runtime_error("Explicit calibration requires EC42 v2.9 firmware");
    auto p=request(wire::Op::CalibrationStatus).payload;
    if(p.size()!=6||(quint8(p[0])>5||p[0]==2)||(p[3]!=0&&p[3]!=3)||quint8(p[4])>1||quint8(p[5])>1)
        throw std::runtime_error("Invalid explicit calibration status");
    calibrationMethod=quint8(p[3]);currentMode=quint8(p[4]);return p;
}
QVector<Sample> DeviceClient::poll(int selected, bool full, bool selectedOnly) {
    if(selectedOnly&&!sampledCell(selected))throw std::runtime_error("Selected cell requires a sampled full snapshot first");
    if (!rows)
        throw std::runtime_error("HELLO required");
    QElapsedTimer budget;
    budget.start();
    auto latch = request(wire::Op::Latch).payload;
    if (latch.size() != 8 || wire::get16(latch, 6) != rows * cols)
        throw std::runtime_error("Invalid snapshot metadata");
    quint16 id = wire::get16(latch, 0);
    if (!id)
        throw std::runtime_error("Invalid snapshot ID");
    double seconds = wire::get32(latch, 2) / 1000.0;
    QVector<Sample> samples = cached.size() == rows * cols ? cached : QVector<Sample>(rows * cols);
    full |= cached.isEmpty();
    for (int start = selectedOnly?selected:0; start < samples.size(); start += 5) {
        if (budget.elapsed() > 3000) throw std::runtime_error("Snapshot page phase exceeded 3-second budget");
        int count = selectedOnly?1:qMin(5, int(samples.size()) - start);
        QByteArray req;
        wire::put16(req, id);
        wire::put16(req, start);
        req.append(char(count));
        auto p = request(wire::Op::ReadPage, req).payload;
        if (p.size() != 5 + 3 * count || wire::get16(p, 0) != id || wire::get16(p, 2) != start ||
            quint8(p[4]) != count)
            throw std::runtime_error("Invalid sample page");
        for (int j = 0; j < count; ++j) {
            auto &s = samples[start + j];
            auto flags = quint8(p[7 + j * 3]);
            s.raw = wire::get16(p, 5 + j * 3);
            if (((flags & 0x70)>0x40) || (flags && !(flags & 1)) || s.raw > 1023)
                throw std::runtime_error("Invalid sample record");
            s.sampled = flags & 1;
            s.active = flags & 2;
            s.debounced = flags & 4;
            s.calibrated = flags & 8;
            s.captureStatus = (flags >> 4)&7;
            s.savedQualified = flags&128;
            if(s.captureStatus>4)throw std::runtime_error("Invalid capture status");
            s.captured = s.captureStatus==1;
            s.seconds = seconds;
        }
        if(selectedOnly)break;
    }
    QVector<int> details;
    if (full) {
        for (int i = 0; i < samples.size(); ++i) details << i;
    } else {
        if (selected >= 0 && selected < samples.size()) details << selected;
        for (int j = 0; !selectedOnly&&j < qMin(12, int(samples.size())); ++j) {
            int i = metadataCursor++ % samples.size();
            if (!details.contains(i)) details << i;
        }
    }
    for (int i : details) {
        if (budget.elapsed() > 3000)
            throw std::runtime_error("Snapshot exceeded 3-second budget");
        QByteArray req;
        wire::put16(req, id);
        wire::put16(req, i);
        auto p = request(wire::Op::ReadCell, req).payload;
        if (p.size() != 20 || wire::get16(p, 0) != id || wire::get16(p, 2) != i)
            throw std::runtime_error("Invalid cell metadata");
        auto &s = samples[i];
        s.metadataKnown = true;
        s.metadataSeconds = seconds;
        s.baseline = wire::get16(p, 4);
        s.bottom = wire::get16(p, 6);
        s.press = wire::get16(p, 8);
        s.release = wire::get16(p, 10);
        s.minimum = wire::get16(p, 12);
        s.maximum = wire::get16(p, 14);
        {
            s.peak = wire::get16(p, 16);
            s.extremum = wire::get16(p, 18);
        }
        if (s.baseline > 1023 || s.bottom > 1023 || s.minimum > 1023 || s.maximum > 1023 || s.peak > 1023 || s.extremum > 1023 ||
            (s.sampled && (s.minimum > s.raw || s.maximum < s.raw)))
            throw std::runtime_error("Invalid cell calibration/range metadata");
    }
    auto decoded=decodeCalibrationRecord(calibrationRecord);
    auto record=decoded.canonical;
    for(auto &s:samples){
        s.mode=currentMode;s.calibrationMethod=calibrationMethod;s.savedRetry=false;
        int i=int(&s-samples.data());
        if(s.sampled&&record.size()==9+samples.size()*2)s.savedRetry=wire::get16(record,9+2*i)&0x2000;
    }
    for(int i:details){
        auto &s=samples[i];if(!s.sampled)continue;
        if(simulated()||!s.calibrated){s.fixedPress=s.press;s.fixedRelease=s.release;continue;}
        QByteArray q;wire::put16(q,i);auto p=request(wire::Op::ActuationGet,q).payload;
        if(p.size()!=20||wire::get16(p,18)!=i||quint8(p[13])>99)throw std::runtime_error("Invalid per-key actuation readback");
        int press=quint8(p[13])?(quint8(p[13])==99?1022:(quint8(p[13])*1023+50)/100):wire::get16(p,6);
        int release=press-(wire::get16(p,6)-wire::get16(p,8));
        s.fixedPress=affineThreshold(press,s.baseline,qMax(s.bottom,s.baseline));
        s.fixedRelease=release>=0?qMin(affineThreshold(release,s.baseline,qMax(s.bottom,s.baseline)),qMax(0,int(s.bottom)-6)):0;
        s.mode=quint8(p[10]);
    }
    cached = samples;
    return samples;
}
} // namespace ec
