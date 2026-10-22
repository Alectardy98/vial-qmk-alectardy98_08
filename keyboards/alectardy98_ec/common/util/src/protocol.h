// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "telemetry.h"
#include "ec42_protocol.h"
#include <QByteArray>
#include <QVector>
#include <memory>
namespace ec {
namespace wire {
constexpr quint8 Version = EC42_MAJOR;
enum class Op : quint8 {
    Hello = EC42_HELLO,
    Latch = EC42_LATCH,
    ReadPage = EC42_READ_PAGE,
    ReadCell = EC42_READ_CELL,
    CalibrationStatus = EC42_CAL_STATUS,
    Config = EC42_CONFIG,
    Backup = EC42_BACKUP,
    RestoreBegin = EC42_RESTORE_BEGIN,
    RestoreChunk = EC42_RESTORE_CHUNK,
    RestoreCommit = EC42_RESTORE_COMMIT,
    Reload = EC42_ABORT,
    FirmwareInfo = EC42_FIRMWARE_INFO,
    CalibrationControl=EC42_CAL_CONTROL, CalibrationValidate=EC42_CAL_VALIDATE,
    CalibrationProposal=EC42_CAL_PROPOSAL, CalibrationActivate=EC42_CAL_ACTIVATE, CalibrationSave=EC42_CAL_SAVE, CaptureEvidence=EC42_CAPTURE_EVIDENCE, Descriptor=EC42_DESCRIPTOR, ActuationGet=EC42_ACTUATION_GET, ActuationSet=EC42_ACTUATION_SET, DynamicRtSet=EC42_RT_SET, ActiveRecord=EC42_ACTIVE_RECORD
};
enum class Status : quint8 { Ok = EC42_OK, Malformed = EC42_MALFORMED, Unsupported = EC42_UNSUPPORTED, Range = EC42_RANGE, Stale = EC42_STALE };
struct Packet {
    Op op = Op::Hello;
    quint16 sequence = 0;
    Status status = Status::Ok;
    bool response = false;
    QByteArray payload;
};
QByteArray encode(const Packet &);
Packet decode(const QByteArray &);
void put16(QByteArray &, quint16);
quint16 get16(const QByteArray &, int offset);
void put32(QByteArray &, quint32);
quint32 get32(const QByteArray &, int offset);
} // namespace wire
class Transport {
  public:
    virtual ~Transport() = default;
    virtual QByteArray exchange(const QByteArray &report) = 0;
};
// Offline simulator; physical transport is implemented separately in device.cpp.
class MockTransport : public Transport {
  public:
    MockTransport(int rows, int cols, const QVector<bool> &mask);
    QByteArray exchange(const QByteArray &report) override;
    void advance(double seconds) {
        simulator.step(seconds);
    }
    void setSampleUnmapped(bool enabled, const QVector<bool> &mapped) {
        simulator.setSampleUnmapped(enabled, mapped);
    }

  private:
    Simulator simulator;
    QVector<Sample> snapshot;
    quint16 snapshotId = 0;
    quint32 millis = 0;
};
class DeviceClient {
  public:
    explicit DeviceClient(Transport &transport) : transport(transport) {}
    QPair<int, int> hello();
    QVector<Sample> poll(int selected = -1, bool full = true, bool selectedOnly = false);
    bool sampledCell(int i) const {return i>=0&&i<cached.size()&&cached[i].sampled;}
    wire::Packet request(wire::Op, const QByteArray &payload = {});
    // EC42 v2.9: state/u16 session/method/mode/record-valid. No deadline.
    QByteArray calibrationStatus();
    void setCalibrationRecord(QByteArray record) { calibrationRecord=std::move(record); }
    int recordBytes() const {return EC42_RECORD_BYTES(rows,cols);}
    int minorVersion() const {
        return minor;
    }
    bool storageReady() const { return storageAvailable; }
    quint16 recordMagic() const { return storageMagic; }
    bool simulated() const {
        return capabilities & 1;
    }
    bool supportsExplicitCalibration() const { return minor==EC42_MINOR && (capabilities&0x40); }
    bool supportsVariableActuation() const {return minor==EC42_MINOR && supportsExplicitCalibration();}
    bool supportsRestore() const {
        return capabilities & 0x20;
    }

  private:
    Transport &transport;
    quint16 sequence = 0;
    int rows = 0, cols = 0, minor = 0;
    quint16 capabilities = 0, storageMagic = 0;
    QVector<Sample> cached;
    int metadataCursor = 0;
    QByteArray calibrationRecord;
    bool storageAvailable = true; // Descriptor health, independent of calibration validity.
    int calibrationMethod=0,currentMode=0;
};
} // namespace ec
