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
// Adapted Qt monitor/window concepts from Cap-Util mainwindow.cpp and signal_level.cpp.
// Copyright 2020 Purdea Andrei (original GPL-2.0-or-later).
// New implementation Copyright 2026 QMK EC Utility contributors.
#include "mainwindow.h"
#include "calibration.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <hidapi.h>
namespace ec {
MainWindow::MainWindow(QWidget *p, bool live, std::unique_ptr<DeviceProvider> provider)
    : QMainWindow(p), liveMode(live) {
    setWindowTitle("QMK EC Utility — simulator only");
    resize(1350, 930);
    auto file = menuBar()->addMenu("&File");
    auto open = file->addAction("&Load Vial JSON…");
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this]() {
        auto path = QFileDialog::getOpenFileName(this, "Load Vial JSON", {}, "JSON (*.json)");
        if (!path.isEmpty() && !loadLayout(path))
            QMessageBox::critical(this, "Layout rejected", error);
    });
    file->addAction("Quit", QKeySequence::Quit, this, &QWidget::close);
    auto help = menuBar()->addMenu("Help");
    help->addAction("About", this, [this]() {
        QMessageBox::about(this, "QMK EC Utility",
                           QString("Standalone Cap-Util derivative; GPL-3.0-or-later.\nOriginal Cap-Util: "
                                   "Copyright 2020 Purdea Andrei.\nQt %1; HIDAPI %2.\nLive mode discovers "
                                   "supported EC devices. --offline disables all USB access.")
                               .arg(qVersion(), QString::fromLatin1(hid_version_str())));
    });
    auto central = new QWidget;
    auto root = new QVBoxLayout(central);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(central);
    setCentralWidget(scroll);
    banner = new QLabel("SIMULATION ONLY — no physical keyboard access. Calibration values are "
                        "synthetic; no settings are written.");
    banner->setStyleSheet("padding:8px;background:#f5d888;color:#171717;font-weight:bold");
    banner->setWordWrap(true);
    root->addWidget(banner);
    deviceLabel = new QLabel("Load a Vial JSON using File → Load Vial JSON");
    deviceLabel->setTextFormat(Qt::PlainText);
    deviceLabel->setWordWrap(true);
    root->addWidget(deviceLabel);
    setupLivePanel(root);
    auto controls = new QHBoxLayout;
    root->addLayout(controls);
    controls->addWidget(new QLabel("Overlay:"));
    overlay = new QComboBox;
    overlay->addItems({"Current", "Baseline", "Bottom", "Press / release", "Min / max", "Press margin"});
    controls->addWidget(overlay);
    pause = new QCheckBox("Pause simulation");
    controls->addWidget(pause);
    sampleUnmapped = new QCheckBox("Simulate sampling unmapped positions");
    controls->addWidget(sampleUnmapped);
    auto reset = new QPushButton(liveMode ? "Clear graph history (not device extrema)" : "Restart simulation / clear history");
    clearHistory = reset;
    controls->addWidget(reset);
    controls->addStretch();
    optionLayout = new QHBoxLayout;
    root->addLayout(optionLayout);
    presets = new QComboBox;
    presets->addItem("Preset: manual");
    optionLayout->addWidget(presets);
    connect(presets, &QComboBox::activated, this, [this](int i) {
        if (i == 0)
            return;
        auto values = layout.presets.value(presets->itemText(i));
        if (values.size() != choices.size())
            return;
        choices = values;
        for (int j = 0; j < choices.size(); ++j) {
            QSignalBlocker block(optionControls[j]);
            optionControls[j]->setCurrentIndex(choices[j]);
        }
        rebuildKeys();
    });
    auto split = new QSplitter(Qt::Vertical);
    root->addWidget(split, 1);
    auto tabs = new QTabWidget;
    split->addWidget(tabs);
    keyboard = new KeyboardView;
    matrix = new MatrixView;
    tabs->addTab(keyboard, "Physical keyboard");
    tabs->addTab(matrix, "Complete matrix");
    auto lower = new QWidget;
    auto lowerLayout = new QVBoxLayout(lower);
    details = new QLabel("Select a key");
    details->setWordWrap(true);
    details->setTextFormat(Qt::PlainText);
    lowerLayout->addWidget(details);
    graph = new SignalGraph;
    lowerLayout->addWidget(graph, 1);
    split->addWidget(lower);
    split->setSizes({450, 290});
    messages = new QPlainTextEdit;
    messages->setReadOnly(true);
    messages->setMaximumBlockCount(200);
    messages->setMaximumHeight(75);
    root->addWidget(messages);
    connect(keyboard, &KeyboardView::keySelected, this, &MainWindow::selectKey);
    connect(matrix, &MatrixView::keySelected, this, &MainWindow::selectKey);
    connect(overlay, &QComboBox::currentTextChanged, this, [this]() { updateDisplays(); });
    connect(sampleUnmapped, &QCheckBox::toggled, this, [this](bool enabled) {
        if (transport) {
            transport->setSampleUnmapped(enabled, layout.mapped());
            tick();
        }
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (liveMode) {
            if (!calibrationActive) { histories = QVector<History>(layout.rows * layout.cols); updateDisplays(); }
            return;
        }
        if (layout.rows) {
            transport = std::make_unique<MockTransport>(layout.rows, layout.cols, layout.mapped());
            transport->setSampleUnmapped(sampleUnmapped->isChecked(), layout.mapped());
            client = std::make_unique<DeviceClient>(*transport);
            client->hello();
            histories = QVector<History>(layout.rows * layout.cols);
            seconds = 0;
            tick();
        }
    });
    timer.setInterval(50);
    connect(&timer, &QTimer::timeout, this, [this]() {
        if (!pause->isChecked())
            tick();
    });
    timer.start();
    if (liveMode) {
        setWindowTitle("QMK EC Utility — calibration and diagnostics");
        banner->hide(); // Keep simulation warning offline; connection/error status lives in the normal UI.
        worker = new SessionWorker(std::move(provider), this);
        connect(worker, &SessionWorker::devicesChanged, this, [this](const QVector<DeviceInfo> &list) {
            QSignalBlocker block(deviceSelector);
            deviceSelector->clear();
            for (const auto &i : list)
                deviceSelector->addItem(i.name + " [" + QString::fromLatin1(i.uid.toHex()) + "] " + i.serial,
                                        i.path);
            for (int i = 0; i < deviceSelector->count(); ++i)
                if (deviceSelector->itemData(i).toByteArray() == currentDevice.path)
                    deviceSelector->setCurrentIndex(i);
            if (list.isEmpty())
                deviceSelector->addItem("No verified EC keyboard — close Vial; see connection log");
        });
        connect(worker, &SessionWorker::identified, this, &MainWindow::connected);
        connect(worker, &SessionWorker::frame, this, &MainWindow::receiveFrame);
        connect(worker, &SessionWorker::actuationStatus, this, &MainWindow::receiveActuation);
        connect(worker, &SessionWorker::completed, this, &MainWindow::commandCompleted);
        connect(worker, &SessionWorker::message, this,
                [this](QString text) { messages->appendPlainText(text); });
        connect(worker, &SessionWorker::connectionLost, this, [this](QString why) {
            vaPending.clear();vaState.clear();vaDebounce.stop();vaBackedSession=false;
            hardwareActive = false;
            layoutReady = false;
            calibrationActive = false;
            commandBusy = false;
            waitingForBackup = false;
            exportingBackup = false;
            currentDevice.generation = 0;
            calibrationAction.clear(); calibrationRequestPending = cancelQueued = captureFinishedPending = false;
            reviewData.clear();firmwareCalibrationState=0;
            previousCalibration.clear(); frameAge.invalidate();
            deviceLabel->setText("Disconnected — no active hardware session");
            settingsStatus->setText("Disconnected. Previous write outcome/persistence unverified.");
            calibrationOverview->setText("No current calibration snapshot; reconnect to inspect.");
            rtSafety->setText("No current telemetry; settings disabled.");
            keyboard->setCapturing(false);
            calibrationLabel->setText("Disconnected — calibration outcome may be unknown. Reconnect, inspect "
                                      "saved values; do not automatically retry writes.");
            samples.clear();
            histories = QVector<History>(histories.size());
            graph->setHistory({});
            keyboard->setSamples(QVector<Sample>(layout.rows * layout.cols), -1, "Current");
            matrix->setSamples(QVector<Sample>(layout.rows * layout.cols));
            messages->appendPlainText(why);
            enableLiveControls();
        });
        connect(deviceSelector, &QComboBox::activated, this, [this](int i) {
            if (worker && i >= 0)
                worker->selectDevice(deviceSelector->itemData(i).toByteArray());
        });
        worker->start();
    }
    enableLiveControls();
}
bool MainWindow::loadLayout(const QString &filename) {
    try {
        if (hardwareActive && (firmwareCalibrationState || calibrationRequestPending))
            throw std::runtime_error("Finish or cancel capture before changing layouts");
        Layout candidate = LayoutParser::load(filename);
        if (hardwareActive) {
            if (candidate.rows != currentDevice.rows || candidate.cols != currentDevice.cols)
                throw std::runtime_error("Layout dimensions do not match the connected EC firmware");
            QFile identityFile(filename);
            if (!identityFile.open(QIODevice::ReadOnly))
                throw std::runtime_error("Layout file could not be re-read");
            auto identity = QJsonDocument::fromJson(identityFile.readAll()).object();
            for (auto field : {QString("vendorId"), QString("productId")})
                if (identity.contains(field)) {
                    bool ok = false;
                    uint value = identity[field].isDouble() ? uint(identity[field].toInt(-1))
                                                            : identity[field].toString().toUInt(&ok, 0);
                    if (identity[field].isDouble())
                        ok = value <= 65535;
                    if (!ok || value != (field == "vendorId" ? currentDevice.vid : currentDevice.pid))
                        throw std::runtime_error("Local layout VID/PID does not match this device");
                }
        }
        auto nextTransport =
            std::make_unique<MockTransport>(candidate.rows, candidate.cols, candidate.mapped());
        nextTransport->setSampleUnmapped(sampleUnmapped->isChecked(), candidate.mapped());
        auto nextClient = std::make_unique<DeviceClient>(*nextTransport);
        auto dims = nextClient->hello();
        if (dims.first != candidate.rows || dims.second != candidate.cols)
            throw std::runtime_error("Device/layout dimension mismatch");
        // Reject unsupported topology before committing a new document.
        auto nextChoices = QVector<int>(candidate.options.size(), 0);
        candidate.visible(nextChoices);
        client.reset();
        transport = std::move(nextTransport);
        client = std::move(nextClient);
        layout = std::move(candidate);
        choices = std::move(nextChoices);
        selected = 0;
        if (worker) worker->selectCell(selected);
    if(hardwareActive&&(currentDevice.ecMinor==EC42_MINOR)&&samples.value(selected).sampled){QByteArray q;wire::put16(q,selected);runCommand("vaRead",q);}
        seconds = 0;
        histories = QVector<History>(layout.rows * layout.cols);
        while (optionLayout->count() > 1) {
            auto item = optionLayout->takeAt(1);
            delete item->widget();
            delete item;
        }
        optionControls.clear();
        for (int i = 0; i < layout.options.size(); ++i) {
            auto container = new QWidget;
            auto box = new QVBoxLayout(container);
            box->setContentsMargins(0, 0, 0, 0);
            auto caption = new QLabel(layout.options[i].name);
            caption->setTextFormat(Qt::PlainText);
            box->addWidget(caption);
            auto combo = new QComboBox;
            combo->addItems(layout.options[i].choices);
            box->addWidget(combo);
            optionControls << combo;
            optionLayout->addWidget(container);
            connect(combo, &QComboBox::currentIndexChanged, this, [this, i](int c) { setOption(i, c); });
        }
        {
            QSignalBlocker b(presets);
            presets->clear();
            presets->addItem("Preset: manual");
            for (auto it = layout.presets.begin(); it != layout.presets.end(); ++it)
                presets->addItem(it.key());
        }
        matrix->setMatrix(layout);
        messages->setPlainText(layout.warnings.join('\n'));
        if (layout.warnings.isEmpty())
            messages->setPlainText(
                "Layout parsed successfully. Unmapped positions are unsampled by default in this simulator.");
        deviceLabel->setText(QString("%1 | Mock EC42 v2.9 | %2 × %3 (%4 positions) | %5\nMock dimensions "
                                     "come from this JSON; no firmware was queried.")
                                 .arg(layout.name)
                                 .arg(layout.rows)
                                 .arg(layout.cols)
                                 .arg(layout.rows * layout.cols)
                                 .arg(filename));
        rebuildKeys();
        tick();
        error.clear();
        if (hardwareActive) {
            layoutReady = true;
            QSettings().setValue("layouts/" + QString::fromLatin1(currentDevice.uid.toHex()), filename);
            try {
                auto c = VialReader::unpackOptions(layout, deviceOptions);
                layout.visible(c);
                choices = c;
                for (int i = 0; i < c.size(); ++i) {
                    QSignalBlocker b(optionControls[i]);
                    optionControls[i]->setCurrentIndex(c[i]);
                }
                rebuildKeys();
            } catch (const std::exception &e) {
                messages->appendPlainText(QString("Layout option warning: %1").arg(e.what()));
            }
            deviceLabel->setText(currentDevice.name + " | REAL | " + currentDevice.firmware + " | UID " +
                                 QString::fromLatin1(currentDevice.uid.toHex()));
            runCommand("read");
        }
        enableLiveControls();
        return true;
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
        statusBar()->showMessage("Layout rejected: " + error);
        return false;
    }
}
void MainWindow::setOption(int index, int choice) {
    if(hardwareActive&&(firmwareCalibrationState||commandBusy))return;
    if (index < 0 || index >= choices.size())
        return;
    auto next = choices;
    next[index] = choice;
    try {
        layout.visible(next);
        choices = next;
        {
            QSignalBlocker b(optionControls[index]);
            optionControls[index]->setCurrentIndex(choice);
        }
        presets->setCurrentIndex(0);
        rebuildKeys();
    } catch (const std::exception &e) {
        messages->appendPlainText(QString("Unsupported selected arrangement: %1").arg(e.what()));
        QSignalBlocker b(optionControls[index]);
        optionControls[index]->setCurrentIndex(choices[index]);
    }
}
void MainWindow::rebuildKeys() {
    visibleKeys = layout.visible(choices);
    keyboard->setKeys(visibleKeys, layout.cols);
    refreshInstalledKeys();
    updateDisplays();
}
void MainWindow::selectKey(int row, int col) {
    if (row < 0 || col < 0 || row >= layout.rows || col >= layout.cols)
        return;
    selected = row * layout.cols + col;
    if (worker) worker->selectCell(selected);
    if(hardwareActive&&(currentDevice.ecMinor==EC42_MINOR)&&samples.value(selected).sampled){QByteArray q;wire::put16(q,selected);runCommand("vaRead",q);}
    matrix->setCurrentCell(row, col);
    updateDisplays();
}
void MainWindow::tick() {
    if (liveMode) {
       
        if (hardwareActive && frameAge.isValid() && frameAge.elapsed() > 6000) {
            enableLiveControls(); updateSettingsSafety();
        }
        return;
    }
    if (!client)
        return;
    try {
        seconds += 0.05;
        transport->advance(seconds);
        samples = client->poll();
        for (int i = 0; i < samples.size(); ++i)
            histories[i].append(samples[i]);
        updateDisplays();
    } catch (const std::exception &e) {
        timer.stop();
        error = e.what();
        messages->appendPlainText("Protocol error: " + error);
    }
}
void MainWindow::updateDisplays() {
    if (samples.isEmpty())
        return;
    refreshCaptureView();
    keyboard->setSamples(samples, selected, overlay->currentText());
    matrix->setSamples(samples);
    int sampled = 0;
    for (auto &s : samples)
        sampled += s.sampled;
    statusBar()->showMessage(
        QString((hardwareActive ? "REAL | " : "SIMULATED | ") +
                QString("%1 visible keys | %2/%3 sampled | ") +
                (hardwareActive ? "EC42 cached telemetry" : "EC42 mock transport | no USB I/O"))
            .arg(visibleKeys.size())
            .arg(sampled)
            .arg(samples.size()));
    if (selected < 0 || selected >= samples.size())
        return;
    const auto &s = samples[selected];
    int row = selected / layout.cols, col = selected % layout.cols;
    if(hardwareActive){
        details->setText(QString("Matrix %1,%2 — ").arg(row).arg(col)+(!s.sampled?"Firmware-defined impossible position":captureLabel(s))+
            QString("\nRaw %1 | baseline %2 | qualified peak %3 | saved bottom %4. Only qualified/enabled saved positions type; ADC scanning continues for inactive keys.").arg(s.raw).arg(s.baseline).arg(s.peak).arg(s.bottom)+(s.captureStatus==1&&s.peak==1023?QString(" Sustained ADC rail: clipped travel lower bound, not precise bottom position."):QString{}));
        return;
    }
    if (!s.sampled)
        details->setText(
            QString(hardwareActive
                        ? "Matrix (%1,%2) — UNSAMPLED by firmware; no cached ADC sample. No calibration data."
                        : "Matrix (%1,%2) — UNMAPPED / UNSAMPLED by simulation policy. Enable sampling of "
                          "unmapped positions to inspect a synthetic signal. No calibration data.")
                .arg(row)
                .arg(col));
    else
        details->setText(
            QString("Matrix (%1,%2) — " +
                    QString(hardwareActive ? (s.calibrationMethod==0?"REAL inherited calibration (unverified)":s.calibrated ? "REAL active calibration valid"
                                                           : "REAL calibration fallback/unverified")
                                           : "SIMULATED calibration valid") +
                    " | raw %3 | baseline %4 | bottom %5 | press "
                    "%6 / release %7\nLogical %8 | debounced %9 | min %10 / max %11 | raw − press %12 | "
                    "travel span %13 | firmware-derived decision thresholds")
                .arg(row)
                .arg(col)
                .arg(s.raw)
                .arg(s.baseline)
                .arg(s.calibrationMethod==2?QString("not measured"):QString::number(s.bottom))
                .arg(s.press)
                .arg(s.release)
                .arg(s.active ? "ON" : "off", s.debounced ? "ON" : "off")
                .arg(s.minimum)
                .arg(s.maximum)
                .arg(int(s.raw) - s.press)
                .arg(s.calibrationMethod==2?QString("not measured"):QString::number(int(s.bottom)-s.baseline)));
    if (hardwareActive && s.sampled) {
        details->setText(details->text() + QString("\nCapture peak %1 | RT extremum %2 | metadata age %3 s. "
            "Raw/state refresh each snapshot; other cells rotate. Min/max are firmware lifetime values.")
            .arg(s.peak).arg(s.extremum).arg(qMax(0., s.seconds - s.metadataSeconds), 0, 'f', 1));
    }
    if(hardwareActive&&(currentDevice.ecMinor==EC42_MINOR)&&s.mode==1){
        details->setText(details->text()+QString("\nInitial Variable Actuation: %1 ADC; fixed release floor: %2 ADC (calculated from current calibration/settings). Dynamic RT replaces the plotted boundaries only after initial activation; returning to the floor resets the cycle. Plotted dynamic boundaries and logical state come from firmware; USB snapshots miss fast movements.").arg(s.fixedPress).arg(s.fixedRelease));
    }
    graph->setHistory(histories[selected].values);
}
} // namespace ec
