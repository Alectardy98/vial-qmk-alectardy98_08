// SPDX-License-Identifier: GPL-3.0-or-later
// Original Cap-Util notice retained:
/* Copyright 2020 Purdea Andrei
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// Qt window/device-monitor organization derived from Cap-Util.
// Copyright 2020 Purdea Andrei (original GPL-2.0-or-later).
#pragma once
#include "device.h"
#include "protocol.h"
#include "widgets.h"
#include <QMainWindow>
#include <QTimer>
#include <QMap>
#include <memory>
class QLabel;
class QComboBox;
class QCheckBox;
class QPlainTextEdit;
class QVBoxLayout;
class QHBoxLayout;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QSlider;
namespace ec {
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr, bool live = false,
                        std::unique_ptr<DeviceProvider> provider = {});
    ~MainWindow() override;
    bool loadLayout(const QString &filename);
    bool healthy() const {
        return client != nullptr && !samples.isEmpty();
    }
    int installedCount() const {return installedKeys.size();}
    bool keyInstalled(int row,int col) const {return installedKeys.contains(row*layout.cols+col);}
    int matrixCount() const {
        return samples.size();
    }
    int physicalCount() const {
        return visibleKeys.size();
    }
    Coordinate selectedCoordinate() const {
        return selected < 0 ? Coordinate{} : Coordinate{selected / layout.cols, selected % layout.cols};
    }
    const Layout &currentLayout() const {
        return layout;
    }
    void setOption(int index, int choice);
    QString lastError() const {
        return error;
    }
  public slots:
    void selectKey(int row, int col);
    void tick();

  private:
    Layout layout;
    QVector<int> choices;
    QVector<VisibleKey> visibleKeys;
    QVector<Sample> samples;
    QVector<History> histories;
    std::unique_ptr<MockTransport> transport;
    std::unique_ptr<DeviceClient> client;
    QTimer timer;
    double seconds = 0;
    int selected = -1;
    QString error;
    KeyboardView *keyboard;
    MatrixView *matrix;
    SignalGraph *graph;
    QLabel *deviceLabel, *details;
    QPlainTextEdit *messages;
    QComboBox *overlay, *presets;
    QCheckBox *pause, *sampleUnmapped;
    QCheckBox *releasedOverride = nullptr;
    QHBoxLayout *optionLayout;
    QVector<QComboBox *> optionControls;
    void rebuildKeys();
    void updateDisplays();
    bool liveMode = false, hardwareActive = false, layoutReady = false, calibrationActive = false;
    bool waitingForBackup = false, exportingBackup = false, commandBusy = false;
    DeviceInfo currentDevice;
    quint32 deviceOptions = 0;
    SessionWorker *worker = nullptr;
    QComboBox *deviceSelector = nullptr;
    QSpinBox *rtPress = nullptr,
             *rtRelease = nullptr;
    QLabel *banner = nullptr, *calibrationLabel = nullptr;
    QLabel *calibrationOverview = nullptr, *settingsStatus = nullptr, *rtSafety = nullptr;
    QPushButton *clearHistory = nullptr;
    QElapsedTimer frameAge;
    void updateSettingsSafety();
    QProgressBar *progress = nullptr;
    QVector<QPushButton *> liveButtons;
    QVector<int> expectedKeys;
    QByteArray previousCalibration;
    QString calibrationAction;
    bool calibrationRequestPending = false, cancelQueued = false;
    bool captureFinishedPending = false;
    QSet<int> installedKeys;
    void refreshInstalledKeys();
    void refreshCaptureView();
    bool firmwareRecordReady=false;
    quint8 firmwareCalibrationState=0;
    QByteArray reviewData, backupNextData;
    QSlider *rtSensitivity=nullptr;
    QCheckBox *rtEnabled=nullptr;
    QLabel *rtMapping=nullptr, *rtEffectiveStatus=nullptr;
    void startBackedCommand(const QString &name,const QByteArray &data={});
    void showCalibrationReview(const QByteArray &data);
    QSlider *globalActuation=nullptr,*keyActuation=nullptr;
    QPushButton *resetActuation=nullptr;
    QLabel *vaSelected=nullptr;
    QByteArray vaState;
    QString vaError;
    bool vaBackedSession=false;
    QTimer vaDebounce;
    QMap<QString,QByteArray> vaPending;
    void queueActuation(const QString &,const QByteArray &);
    void receiveActuation(const QByteArray &);
    void setupLivePanel(QVBoxLayout *root);
    void enableLiveControls();
    void runCommand(const QString &name, const QByteArray &data = {});
    void connected(DeviceInfo info, QByteArray definition, quint32 options);
    void receiveFrame(QVector<Sample> values, QByteArray status);
    void commandCompleted(QString name, QByteArray data);
};
} // namespace ec
