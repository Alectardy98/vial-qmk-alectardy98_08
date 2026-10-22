// SPDX-License-Identifier: GPL-3.0-or-later
// HIDAPI worker organization follows Cap-Util's serialized background I/O approach.
#pragma once
#include "layout.h"
#include "protocol.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <functional>
#include <atomic>
namespace ec {
struct DeviceInfo {
    QByteArray path, uid;
    QString name, serial, firmware;
    quint16 vid = 0, pid = 0;
    int rows = 0, cols = 0, ecMinor = 0;
    quint64 generation = 0;
    bool operator==(const DeviceInfo &b) const {
        return path == b.path && uid == b.uid && name == b.name;
    }
};
class VialReader {
  public:
    static DeviceInfo identify(Transport &, DeviceInfo);
    static QByteArray definition(Transport &);
    static quint32 options(Transport &);
    static QVector<int> unpackOptions(const Layout &, quint32);
    static QByteArray decompress(const QByteArray &);
    static QByteArray read(Transport &, const QByteArray &);
};
class DeviceProvider {
  public:
    virtual ~DeviceProvider() = default;
    virtual QVector<DeviceInfo> enumerate() = 0;
    virtual std::unique_ptr<Transport> open(const QByteArray &path) = 0;
};
class HidProvider : public DeviceProvider {
  public:
    ~HidProvider() override;
    static bool isVialCandidate(quint16 page, quint16 usage, const QString &serial);
    QVector<DeviceInfo> enumerate() override;
    std::unique_ptr<Transport> open(const QByteArray &) override;

  private:
    bool initialized = false;
};
struct CalibrationProgress {
    int complete = 0, incomplete = 0, unsampled = 0, invalid = 0;
    bool verified = false;
};
CalibrationProgress calibrationProgress(const QVector<Sample> &, const QVector<int> &expected, bool saved);
quint32 configCrc(const QByteArray &);
void validateBackup(const QByteArray &, int rows, int cols);
QByteArray readBackup(DeviceClient &, int rows, int cols);
QByteArray readActive(DeviceClient &, int rows, int cols);
void restoreBackup(DeviceClient &, const QByteArray &, int rows, int cols);
class SessionWorker : public QThread {
    Q_OBJECT
  public:
    explicit SessionWorker(std::unique_ptr<DeviceProvider> provider = {}, QObject *parent = nullptr);
    ~SessionWorker() override;
    void selectDevice(QByteArray path);
    void selectCell(int index);
    void command(quint64 generation, QString name, QByteArray data = {});
    void stop();
  signals:
    void devicesChanged(QVector<ec::DeviceInfo> devices);
    void identified(ec::DeviceInfo info, QByteArray definition, quint32 options);
    void frame(QVector<ec::Sample> samples, QByteArray status);
    void connectionLost(QString reason);
    void message(QString text);
    void completed(QString command, QByteArray data);
    void actuationStatus(QByteArray data);

  protected:
    void run() override;

  private:
    struct Command {
        quint64 generation;
        QString name;
        QByteArray data;
    };
    std::unique_ptr<DeviceProvider> provider;
    QMutex mutex;
    QQueue<Command> queue;
    QByteArray desired;
    int selectedCell = -1;
    std::atomic<bool> cancelCalibrationRequested{false};
};
} // namespace ec
Q_DECLARE_METATYPE(ec::DeviceInfo)
Q_DECLARE_METATYPE(QVector<ec::DeviceInfo>)
