// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "settings.h"
#include "calibration.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <stdexcept>
namespace ec {
namespace {
bool confirm(QWidget *w,const QString &title,const QString &text){return QMessageBox::question(w,title,text,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;}
}
MainWindow::~MainWindow(){if(worker)worker->stop();}
void MainWindow::setupLivePanel(QVBoxLayout *root){
    auto warning=new QLabel("Please close Vial before using QMK EC Utility.");
    warning->setObjectName("vialWarning");warning->setStyleSheet("padding: 5px; background: #fff0c2; color: #302800;");root->addWidget(warning);
    auto tabs=new QTabWidget;tabs->setMaximumHeight(360);root->addWidget(tabs);
    auto page=[&](QString name){auto w=new QWidget;auto box=new QVBoxLayout(w);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(w);tabs->addTab(scroll,name);return box;};
    auto box=page("Calibration");auto rt=page("Variable Actuation");auto advanced=page("Advanced");
    deviceSelector=new QComboBox;deviceSelector->addItem(liveMode?"Discovering EC42 firmware…":"Offline simulator — no USB");box->addWidget(deviceSelector);
    auto button=[&](QVBoxLayout *where,QString title,QString action,auto fn){auto b=new QPushButton(title);b->setProperty("action",action);liveButtons<<b;where->addWidget(b);connect(b,&QPushButton::clicked,this,fn);return b;};
    auto legend=new QLabel("Gray: uncalibrated/inactive · Blue: saved enabled or NEW unsaved capture · Red: retry · Dark gray −: firmware nonexistent. Press any subset; click a key for details.");legend->setWordWrap(true);box->addWidget(legend);
    auto manual=new QHBoxLayout;box->addLayout(manual);
    // Button groups are independent workflows, with one shared review surface.
    auto add=[&](QHBoxLayout *row,QString title,QString action,auto fn){auto b=new QPushButton(title);b->setProperty("action",action);liveButtons<<b;row->addWidget(b);connect(b,&QPushButton::clicked,this,fn);};
    add(manual,"Start Manual Calibration","start",[this](){
        if(!confirm(this,"Start Manual Calibration","Release ALL physical keys. Capture has no duration limit and never saves automatically. Fully press any keys you want to calibrate, then Finish, review, and Apply & Save. Uncalibrated keys stay disabled; later saves retain previously qualified keys. Saved calibration remains unchanged until approval. Wait until actuation settings report Saved before starting. Disable Dynamic RT first if its offsets cannot fit the new calibration."))return;
        refreshInstalledKeys();
        QByteArray data(1,char(rtEnabled->isChecked()));
        startBackedCommand("start",data);
    });
    add(manual,"Finish Calibration","finish",[this](){runCommand(firmwareCalibrationState==3||firmwareCalibrationState==4?"resume":"finish");});
    add(manual,"Cancel Calibration","cancel",[this](){runCommand("cancel");});
    add(manual,"Retry Selected Key","retry",[this](){if(selected<0)return;QByteArray q;wire::put16(q,selected);runCommand("retry",q);});
    add(manual,"Review Results","review",[this](){showCalibrationReview(reviewData);});
    add(manual,"Apply & Save","applyManual",[this](){
        if(confirm(this,"Apply & Save","Partial calibration: uncalibrated keys and invalid attempts will STOP TYPING after saving. Previously qualified untouched keys are retained. Release every physical key. Apply verified captures and save to this keyboard's EC EEPROM allocation? A disk backup is required first. Only packed-format backups can be restored by matching EC42 firmware. Keep the older firmware and its matching backup separately for rollback. Raw HID backup restoration and bootloader recovery work independently of typing."))startBackedCommand("applyManual",reviewData);
    });
    calibrationLabel=new QLabel("Requires supported EC42 v2.9 firmware.");calibrationLabel->setObjectName("calibrationProgressSummary");calibrationLabel->setWordWrap(true);box->addWidget(calibrationLabel);
    calibrationOverview=new QLabel("Proposed, active and saved values are distinct. No automatic save.");calibrationOverview->setWordWrap(true);box->addWidget(calibrationOverview);
    progress=new QProgressBar;box->addWidget(progress);
    auto vaHelp=new QLabel("Choose how far a calibrated key must be pressed before it activates. The right endpoint, Deepest, uses bottom minus 1 ADC count. Signal percentage is not physical distance; near-bottom settings can miss presses if the measured bottom drifts. The global control changes inheriting keys only; per-key overrides remain unchanged. Reset a selected key to global default to include it. Configure all keys together or adjust individual keys. Percentages refer to calibrated ADC span, not measured physical distance. Variable Actuation is always active for calibrated keys. Release ALL keys to change settings. Changes apply immediately and save after 2 seconds idle; no Apply or Save button.");vaHelp->setWordWrap(true);rt->addWidget(vaHelp);
    rt->addWidget(new QLabel("Global default actuation (percentage; right endpoint = Deepest)"));
    globalActuation=new QSlider(Qt::Horizontal);globalActuation->setObjectName("globalActuation");globalActuation->setRange(1,99);rt->addWidget(globalActuation);
    vaSelected=new QLabel("Select a calibrated key on the keyboard below");vaSelected->setWordWrap(true);rt->addWidget(vaSelected);
    keyActuation=new QSlider(Qt::Horizontal);keyActuation->setObjectName("keyActuation");keyActuation->setRange(1,99);rt->addWidget(keyActuation);
    resetActuation=new QPushButton("Reset selected key to global default");resetActuation->setObjectName("resetActuation");rt->addWidget(resetActuation);
    auto stage=[this](int index,int value){QByteArray q;wire::put16(q,index);q.append(char(value));queueActuation("vaSet",q);};
    connect(globalActuation,&QSlider::valueChanged,this,[stage](int v){stage(65535,v);});
    connect(globalActuation,&QSlider::sliderReleased,this,[this,stage](){if(globalActuation->value()==99&&vaState.size()==20&&wire::get16(vaState,6)!=1022)stage(65535,99);});
    keyActuation->setToolTip("Steps 1–98 are percentages of calibrated ADC span. Right endpoint 99 is Deepest: measured bottom minus 1 ADC. Not a physical-distance scale.");
    connect(keyActuation,&QSlider::valueChanged,this,[this,stage](int v){if(selected>=0)stage(selected,v);});
    connect(resetActuation,&QPushButton::clicked,this,[this,stage](){if(selected>=0)stage(selected,0);});
    vaDebounce.setSingleShot(true);vaDebounce.setInterval(200);
    connect(&vaDebounce,&QTimer::timeout,this,[this](){
        if(!hardwareActive||(currentDevice.ecMinor!=EC42_MINOR)){vaPending.clear();return;}
        if(commandBusy){vaDebounce.start(100);return;}
        if(vaPending.isEmpty())return;
        auto i=vaPending.begin();QString name=i.key().section(':',0,0);QByteArray value=i.value();vaPending.erase(i);if(vaBackedSession)runCommand(name,value);else startBackedCommand(name,value);
        if(!vaPending.isEmpty())vaDebounce.start();
    });
    auto explanation=new QLabel("Variable Actuation still determines the initial activation point. Dynamic Rapid Trigger then uses small changes in key movement to release and reactivate the key without returning to its normal fixed release point. After returning to the fixed release floor, the next activation again requires the Variable Actuation threshold. Higher sensitivity requires less ADC movement. Checking this box applies immediately and automatically saves when safe after 2 seconds idle with all keys released. Telemetry cannot show every scan.");explanation->setWordWrap(true);rt->addWidget(explanation);
    rtEnabled=new QCheckBox("Enable Dynamic Rapid Trigger");rtEnabled->setObjectName("rtEnabled");rt->addWidget(rtEnabled);
    rt->addWidget(new QLabel("Rapid Trigger Sensitivity — Less Sensitive ←→ More Sensitive"));
    rtSensitivity=new QSlider(Qt::Horizontal);rtSensitivity->setObjectName("rtSensitivity");rtSensitivity->setRange(0,0);rt->addWidget(rtSensitivity);
    rtEffectiveStatus=new QLabel("Firmware settings unknown; no changes applied.");rtEffectiveStatus->setObjectName("rtEffectiveStatus");rtEffectiveStatus->setWordWrap(true);rt->addWidget(rtEffectiveStatus);
    rtMapping=new QLabel("Calibrate first; sensitivity is measured in ADC counts, not millimeters.");rtMapping->setWordWrap(true);rt->addWidget(rtMapping);
    auto param=new QGridLayout;advanced->addLayout(param);int row=0;
    auto spin=[&](QString name,int value){auto s=new QSpinBox;s->setRange(1,255);s->setValue(value);param->addWidget(new QLabel(name),row,0);param->addWidget(s,row++,1);return s;};
    rtPress=spin("RT press movement (ADC counts)",70);rtRelease=spin("RT release movement (ADC counts)",70);
    rtPress->setObjectName("rtPress");rtRelease->setObjectName("rtRelease");
    connect(rtEnabled,&QCheckBox::toggled,this,[this](bool on){
        if((currentDevice.ecMinor==EC42_MINOR)&&vaState.size()==20){
            int offset=255-rtSensitivity->value(),lo=255-rtSensitivity->maximum(),hi=255-rtSensitivity->minimum();
            bool valid=rtPress->value()>=lo&&rtPress->value()<=hi&&rtRelease->value()>=lo&&rtRelease->value()<=hi;
            if(on&&!valid)messages->appendPlainText(QString("Enabling RT with movement %1 ADC counts because stored %2/%3 does not fit the current %4–%5 range. No calibration values are changed.").arg(offset).arg(rtPress->value()).arg(rtRelease->value()).arg(lo).arg(hi));
            QByteArray q(1,char(on));q.append(char(on&&!valid?offset:rtPress->value()));q.append(char(on&&!valid?offset:rtRelease->value()));queueActuation("vaRt",q);
        }
    });
    connect(rtSensitivity,&QSlider::valueChanged,this,[this](int value){
        if((currentDevice.ecMinor==EC42_MINOR)){
            {QSignalBlocker p(rtPress),r(rtRelease);rtPress->setValue(255-value);rtRelease->setValue(255-value);}
            QByteArray q(1,char(rtEnabled->isChecked()));q.append(char(255-value));q.append(char(255-value));queueActuation("vaRt",q);return;
        }
    });
    for(auto spin:{rtPress,rtRelease})connect(spin,&QSpinBox::valueChanged,this,[this](){
        if((currentDevice.ecMinor==EC42_MINOR)){QByteArray q(1,char(rtEnabled->isChecked()));q.append(char(rtPress->value()));q.append(char(rtRelease->value()));queueActuation("vaRt",q);}
    });
    settingsStatus=new QLabel("No settings have been written.");settingsStatus->setWordWrap(true);settingsStatus->setObjectName("settingsStatus");rt->addWidget(settingsStatus);
    rtSafety=new QLabel;rtSafety->setWordWrap(true);rtSafety->setObjectName("rtSafety");advanced->addWidget(rtSafety);
    button(advanced,"Enter keyboard bootloader…","bootloader",[this](){
        if(confirm(this,"Enter keyboard bootloader","Discard any unsaved calibration and enter the connected keyboard's native bootloader? No firmware is installed by this action. This recovery command works without any typing keys."))runCommand("bootloader");
    });
    button(advanced,"Read active settings","read",[this](){runCommand("read");});
    button(advanced,"Export EC backup…","backup",[this](){exportingBackup=true;runCommand("backup");});
    button(advanced,"Restore EC backup…","restore",[this](){
        auto path=QFileDialog::getOpenFileName(this,"Restore EC backup",{},"JSON (*.json)");if(path.isEmpty())return;QFile f(path);
        if(!f.open(QIODevice::ReadOnly)||f.size()>65536)return;
        auto o=QJsonDocument::fromJson(f.readAll()).object();auto b=QByteArray::fromHex(o["record"].toString().toLatin1());
        try{if(!decodeCalibrationRecord(b).packed)throw std::runtime_error("Only packed-format backups can be restored. Legacy backups are not supported.");validateBackup(b,currentDevice.rows,currentDevice.cols);if(o["uid"].toString()!=QString::fromLatin1(currentDevice.uid.toHex())||o["crc32"].toString()!=QString::number(configCrc(b),16))throw std::runtime_error("Backup identity/checksum mismatch");
            if(confirm(this,"Restore calibration","Replace only the EC record with this verified backup? UID identifies the keyboard definition, not a unique physical unit: confirm the correct keyboard. Release every physical key. Vial settings are not overwritten."))runCommand("restore",b);
        }catch(const std::exception &e){QMessageBox::warning(this,"Backup rejected",e.what());}
    });
}
void MainWindow::startBackedCommand(const QString &name,const QByteArray &data){
    calibrationAction=name;backupNextData=data;waitingForBackup=true;calibrationRequestPending=true;
    expectedKeys=installedKeys.values();
    runCommand("backup");
}
void MainWindow::enableLiveControls(){
    bool ready=hardwareActive&&layoutReady&&!commandBusy;
    bool capture=firmwareCalibrationState==1;
    auto review=QJsonDocument::fromJson(reviewData).object();int method=review["method"].toInt();
    for(auto b:liveButtons){auto a=b->property("action").toString();bool on=ready;
        if(a=="finish"){on&=firmwareCalibrationState==1||firmwareCalibrationState==3||firmwareCalibrationState==4;b->setText(firmwareCalibrationState==3||firmwareCalibrationState==4?"Continue Calibration":"Finish Calibration");}
        else if(a=="cancel")on=hardwareActive&&layoutReady&&!cancelQueued&&(firmwareCalibrationState||commandBusy||waitingForBackup);
        else if(a=="review")on&=!reviewData.isEmpty();
        else if(a=="applyManual"){
            on&=(firmwareCalibrationState==3||firmwareCalibrationState==4)&&review["canApply"].toBool()&&method==3&&summarizeCapture(samples,installedKeys).canApply()&&frameAge.isValid()&&frameAge.elapsed()<6000;
            auto keys=review["keys"].toArray();
            for(int i=0;i<samples.size()&&i<keys.size();++i){auto key=keys[i].toObject();if(samples[i].sampled&&key["enabled"].toBool())on&=key.contains("candidateRelease")&&samples[i].raw<key["candidateRelease"].toInt();}
            b->setToolTip(on?"Firmware validation passed; save is explicit and read back.":"Finish/review must validate at least one NEW captured key and all retained enabled keys. Enabled readings must be below proposed release thresholds. No automatic save.");
        }
        else if(a=="start")on&=!installedKeys.isEmpty()&&frameAge.isValid()&&!firmwareCalibrationState&&((currentDevice.ecMinor!=EC42_MINOR)||vaState.size()!=20||(vaState[4]==0||vaState[4]==4));
        else if(a=="retry")on&=firmwareCalibrationState==1&&selected>=0&&installedKeys.contains(selected);
        else on&=!capture;
        b->setEnabled(on);
    }
    for(auto w:{rtPress,rtRelease})w->setEnabled(ready&&!firmwareCalibrationState);
    rtEnabled->setEnabled(ready&&!firmwareCalibrationState);
    for(auto c:optionControls)c->setEnabled(!firmwareCalibrationState&&!commandBusy);
    presets->setEnabled(!firmwareCalibrationState&&!commandBusy);deviceSelector->setEnabled(!capture&&!commandBusy);
    if(pause)pause->setEnabled(!liveMode);
    if(sampleUnmapped)sampleUnmapped->setEnabled(!liveMode);

    updateSettingsSafety();
}
void MainWindow::runCommand(const QString &name,const QByteArray &data){
    if(!worker||!hardwareActive||!layoutReady||(commandBusy&&name!="cancel"))return;
    if(name=="cancel"){cancelQueued=true;waitingForBackup=false;calibrationAction.clear();backupNextData.clear();calibrationRequestPending=false;}
    if(name=="start")reviewData.clear();
    commandBusy=true;enableLiveControls();worker->command(currentDevice.generation,name,data);
}
void MainWindow::connected(DeviceInfo info, QByteArray definition, quint32 options) {
    vaState.clear();vaPending.clear();vaDebounce.stop();vaBackedSession=false;
    hardwareActive = true;
    layoutReady = false;
    commandBusy = false;
    calibrationActive = false;
    currentDevice = info;
    frameAge.invalidate();
    deviceOptions = options;
    previousCalibration.clear(); calibrationAction.clear();
    calibrationRequestPending = cancelQueued = captureFinishedPending = false;
    reviewData.clear();firmwareCalibrationState=0;
    keyboard->setHardware(true);
    matrix->setHardware(true);
    graph->setHardware(true);
    QString path;
    if (!definition.isEmpty()) {
        auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/layouts";
        QDir().mkpath(dir);
        path = dir + "/" + QString::fromLatin1(info.uid.toHex()) + ".json";
        QSaveFile f(path);
        if (f.open(QIODevice::WriteOnly) && f.write(definition) == definition.size())
            f.commit();
        else
            path.clear();
    }
    if (path.isEmpty()) {
        path = QSettings().value("layouts/" + QString::fromLatin1(info.uid.toHex())).toString();
        if (path.isEmpty())
            path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/layouts/" +
                   QString::fromLatin1(info.uid.toHex()) + ".json";
    }
    if (!loadLayout(path)) {
        layoutReady = false;
        messages->appendPlainText(
            "Automatic layout unavailable: " + error +
            ". Use File → Load Vial JSON; a matching file association will be remembered.");
    } else {
        try {
            auto selected = VialReader::unpackOptions(layout, options);
            layout.visible(selected);
            choices = selected;
            for (int i = 0; i < choices.size(); ++i) {
                QSignalBlocker b(optionControls[i]);
                optionControls[i]->setCurrentIndex(choices[i]);
            }
            rebuildKeys();
        } catch (const std::exception &e) {
            messages->appendPlainText(
                QString("Device layout option warning: %1; showing defaults").arg(e.what()));
        }
        runCommand("read");
    }
    deviceLabel->setText(info.name + QString(" | EC42 v2.%1 | UID ").arg(info.ecMinor) + QString::fromLatin1(info.uid.toHex()) +
                         QString(" | %1×%2 | ").arg(info.rows).arg(info.cols) + info.serial + " | " +
                         info.firmware);
    calibrationLabel->setText("Connected to EC42 firmware. Manual captures require Apply & Save; actuation changes save automatically after released-key idle.");
    enableLiveControls();
}

void MainWindow::receiveFrame(QVector<Sample> values,QByteArray status){
    if(!hardwareActive||!layoutReady||values.size()!=layout.rows*layout.cols||status.size()!=6)return;
    frameAge.restart();firmwareRecordReady=bool(status[5]);firmwareCalibrationState=quint8(status[0]);calibrationActive=firmwareCalibrationState==1;
    samples=std::move(values);for(int i=0;i<samples.size();++i)if(histories[i].values.isEmpty()||histories[i].values.last().seconds!=samples[i].seconds)histories[i].append(samples[i]);
    refreshInstalledKeys();refreshCaptureView();
    calibrationOverview->setText(QString("Active method %1 | firmware state %2 | record/storage readiness %3. Proposed data is never saved automatically.")
        .arg(quint8(status[3])).arg(firmwareCalibrationState).arg(status[5]?(status[3]?"valid":"inherited / no new checksum"):"NOT READY — inspect storage/calibration; no write retries"));
    updateDisplays();enableLiveControls();
}
void MainWindow::commandCompleted(QString name,QByteArray data){
    if(name=="vaSet"||name=="vaRt"||name=="vaRead"){
        commandBusy=false;calibrationRequestPending=false;vaBackedSession=true;vaError.clear();receiveActuation(data);enableLiveControls();return;
    }

    commandBusy=cancelQueued&&name!="cancel";
    if(name=="error"){
        if(data.startsWith("va")){vaPending.clear();vaDebounce.stop();vaError=QString::fromUtf8(data);settingsStatus->setText("Error: "+vaError);}

        waitingForBackup=false;calibrationAction.clear();calibrationRequestPending=false;
        calibrationLabel->setText("Command failed: "+QString::fromUtf8(data)+". No automatic retry; inspect state/readback.");messages->appendPlainText(calibrationLabel->text());
    }else if(name=="backup"){
        bool restorable=false;
        try{if(decodeCalibrationRecord(data).packed){validateBackup(data,currentDevice.rows,currentDevice.cols);restorable=true;}}catch(const std::exception &){}
        if(!restorable)messages->appendPlainText("Raw EEPROM bytes archived for recovery/investigation. This invalid or unsupported record cannot be restored; use a valid EC42 v2.9 backup or recalibrate.");
        if(waitingForBackup){
            previousCalibration=data;waitingForBackup=false;QString action=calibrationAction;calibrationAction.clear();
            auto dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/backups";QDir().mkpath(dir);
            auto path=dir+"/"+QString::fromLatin1(currentDevice.uid.toHex())+"-"+QString::number(QDateTime::currentMSecsSinceEpoch())+".json";
            QJsonObject o{{"format","qmk-ec-backup-1"},{"restorable",restorable},{"uid",QString::fromLatin1(currentDevice.uid.toHex())},{"serial",currentDevice.serial},{"rows",currentDevice.rows},{"cols",currentDevice.cols},{"crc32",QString::number(configCrc(data),16)},{"record",QString::fromLatin1(data.toHex())}};
            QSaveFile f(path);auto bytes=QJsonDocument(o).toJson();
            if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){calibrationLabel->setText("Operation blocked: disk backup could not be saved.");calibrationRequestPending=false;}
            else{messages->appendPlainText("Recovery backup: "+path);if(!cancelQueued)runCommand(action,backupNextData);}
        }else if(exportingBackup){
            exportingBackup=false;auto path=QFileDialog::getSaveFileName(this,"Export EC backup",{},"JSON (*.json)");if(!path.isEmpty()){
                QJsonObject o{{"format","qmk-ec-backup-1"},{"restorable",restorable},{"uid",QString::fromLatin1(currentDevice.uid.toHex())},{"serial",currentDevice.serial},{"rows",currentDevice.rows},{"cols",currentDevice.cols},{"crc32",QString::number(configCrc(data),16)},{"record",QString::fromLatin1(data.toHex())}};
                QSaveFile f(path);auto bytes=QJsonDocument(o).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())QMessageBox::warning(this,"Export failed",f.errorString());
            }
        }
    }else if(name=="start"){
        calibrationRequestPending=false;firmwareCalibrationState=1;calibrationActive=true;for(auto &s:samples){s.captureStatus=0;s.captured=false;}progress->setRange(0,expectedKeys.size());progress->setValue(0);updateDisplays();
        calibrationLabel->setText("Manual capture running until Finish or Cancel. Saved values unchanged.");
    }else if(name=="finish"){
        calibrationRequestPending=false;calibrationActive=false;firmwareCalibrationState=3;
        progress->setRange(0,expectedKeys.size());progress->setValue(0);
        auto o=QJsonDocument::fromJson(data).object();
        auto keys=o["keys"].toArray();
        reviewData=QJsonDocument(o).toJson(QJsonDocument::Compact);
        calibrationLabel->setText(o["canApply"].toBool()?"Results ready for review. No active/saved calibration changes yet.":"No valid NEW capture or unsafe retained calibration: cannot apply. Review and retry.");
        if(!o["validationError"].toString().isEmpty())calibrationLabel->setText("Firmware validation blocked saving: "+o["validationError"].toString()+". Check retained enabled calibration and Advanced settings.");
        showCalibrationReview(reviewData);
    }else if(name=="resume"){
        reviewData.clear();firmwareCalibrationState=1;calibrationActive=true;updateDisplays();
    }else if(name=="retry"){
        if(data.size()==2){int index=wire::get16(data,0);if(index<samples.size()){samples[index].captureStatus=0;samples[index].captured=false;}}updateDisplays();
    }else if(name=="cancel"||name=="reload"){
        cancelQueued=commandBusy=calibrationRequestPending=false;reviewData.clear();firmwareCalibrationState=0;calibrationActive=false;for(auto &s:samples){s.captureStatus=0;s.captured=false;}progress->setRange(0,expectedKeys.size());progress->setValue(0);
        calibrationLabel->setText("Calibration cancelled. Unsaved changes discarded; saved calibration preserved.");
    }else if(name=="applyManual"||name=="restore"){
        vaBackedSession=false;
        reviewData.clear();firmwareCalibrationState=0;calibrationActive=false;
        calibrationLabel->setText("Active and EEPROM-driver readback verified. Power-cycle persistence still requires physical testing.");runCommand("read");
    }else if(name=="read"&&data.size()==9){
        QSignalBlocker p(rtPress),r(rtRelease),e(rtEnabled);
        rtEnabled->setChecked(data[0]);rtPress->setRange(1,255);rtRelease->setRange(1,255);
        rtPress->setValue(quint8(data[7]));rtRelease->setValue(quint8(data[8]));
        settingsStatus->setText("Read active RAM settings; no writes performed. Check persistence status separately.");
    }
    updateDisplays();enableLiveControls();
}
void MainWindow::updateSettingsSafety(){
    if(!rtSafety)return;
    if((currentDevice.ecMinor==EC42_MINOR)){
        bool ready=hardwareActive&&layoutReady&&firmwareRecordReady&&!commandBusy&&!firmwareCalibrationState&&vaState.size()==20&&vaState[5]&&quint8(vaState[4])!=3&&frameAge.isValid()&&frameAge.elapsed()<6000;
        int gap=vaState.size()==20?wire::get16(vaState,6)-wire::get16(vaState,8):1023;
        int minimum=1;while(minimum<99&&(minimum*1023+50)/100<=gap)++minimum;
        auto safeMinimum=[&](int span){int v=minimum;if(span<100)return v;while(v<99){int q=(v*1023+50)/100;if((q-gap)*span/1023>0&&q*span/1023>(q-gap)*span/1023)break;++v;}return v;};
        int allMinimum=minimum,selectedMinimum=minimum;
        for(int i=0;i<samples.size();++i){const auto &s=samples[i];if(!s.sampled||!s.calibrated||!s.metadataKnown)continue;int m=safeMinimum(int(s.bottom)-s.baseline);allMinimum=qMax(allMinimum,m);if(i==selected)selectedMinimum=m;}
        {QSignalBlocker a(globalActuation),b(keyActuation);globalActuation->setMinimum(allMinimum);keyActuation->setMinimum(selectedMinimum);}
        globalActuation->setEnabled(ready);bool key=selected>=0&&selected<samples.size()&&samples[selected].sampled&&samples[selected].calibrated;
        keyActuation->setEnabled(ready&&key);resetActuation->setEnabled(ready&&key);
        rtEnabled->setEnabled(ready);
        int lo=4,hi=255;bool any=false;
        for(const auto &s:samples)if(s.sampled&&s.calibrated&&s.metadataKnown){any=true;hi=qMin(hi,int(s.bottom)-s.fixedRelease-2);}
        {QSignalBlocker b(rtSensitivity);rtSensitivity->setRange(255-qMax(lo,hi),255-lo);}
        rtSensitivity->setEnabled(ready&&any&&hi>=lo);
        rtPress->setEnabled(ready);rtRelease->setEnabled(ready);
        rtEnabled->setEnabled(ready&&(rtEnabled->isChecked()||(any&&hi>=lo)));
        rtMapping->setText(QString("Global movement range %1–%2 ADC counts above the fixed release floor, filtered by median-of-three. The 4-count minimum is a conservative software floor, not a measured per-key noise guarantee. More sensitive = smaller movement. If sensor noise is comparable to this range, leave RT off and investigate; calibration does not prove a noise-free signal. Firmware revalidates every enabled key. Initial press uses Variable Actuation; fixed release resets the RT cycle.").arg(lo).arg(hi));
        rtSafety->setText("Deepest uses bottom minus 1 ADC; each release floor stays at least 6 counts below bottom. RT offsets that no longer fit are clamped after an explicit VA change and shown by readback. Settings updates are rejected while a key is held or a calibration is active. No automatic retry on error. Release keys, then move the control again.");
        return;
    }
    globalActuation->setEnabled(false);keyActuation->setEnabled(false);resetActuation->setEnabled(false);
    rtEnabled->setEnabled(false);rtSensitivity->setEnabled(false);rtPress->setEnabled(false);rtRelease->setEnabled(false);
    rtSafety->setText("Connect supported EC42 v2.9 firmware to configure actuation.");
}

void MainWindow::showCalibrationReview(const QByteArray &data){
    auto o=QJsonDocument::fromJson(data).object();if(o.isEmpty())return;
    int method=o["method"].toInt();auto keys=o["keys"].toArray();auto record=decodeCalibrationRecord(QByteArray::fromHex(o["record"].toString().toLatin1())).canonical;
    QDialog dialog(this);dialog.setWindowTitle("Calibration review — proposed values only");dialog.resize(1100,600);auto box=new QVBoxLayout(&dialog);
    auto note=new QLabel(method==3?"Manual peaks use minima of complete 16-scan blocks and need ≥100 ADC counts of observed movement. Hold each key fully for about a second, then release. Manual range shows qualified released/pressed bounds, not raw extrema. A sustained ADC rail is a measured lower bound; a scan counter capped at 255 is normal. Nothing saved.":o["limitation"].toString());note->setWordWrap(true);box->addWidget(note);
    auto table=new QTableWidget(0,10);table->setHorizontalHeaderLabels({"Matrix","Baseline","Qualified bottom","Mean / SD","Observed range","Press ADC","Release ADC","Samples","Assessment","Confidence"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int i=0;i<samples.size();++i){if(!samples[i].sampled)continue;const auto &s=samples[i];QStringList v{QString("%1,%2").arg(i/layout.cols).arg(i%layout.cols),QString::number(s.baseline)};
        if(method==3&&record.size()==9+samples.size()*2){
            auto key=i<keys.size()?keys[i].toObject():QJsonObject{};int b=wire::get16(record,9+i*2)&1023;
            v<<(key["preserved"].toBool()?QString("saved %1").arg(b):key["qualified"].toBool()?QString::number(b):QString("—"))<<"—"<<QString("%1..%2").arg(key["minimum"].toInt()).arg(key["maximum"].toInt())
                <<(key.contains("candidatePress")?QString::number(key["candidatePress"].toInt()):"—")
                <<(key.contains("candidateRelease")?QString::number(key["candidateRelease"].toInt()):"—")
                <<QString("%1%2").arg(key["samplesLowerBound"].toInt()).arg(key["samplesLowerBound"].toInt()==255?"+ scans":" scans")
                <<key["result"].toString()<<(key["railSeen"].toBool()?"ADC rail observed; inspect signal history":"Physical bottom press required");
        }int row=table->rowCount();table->insertRow(row);for(int col=0;col<v.size();++col)table->setItem(row,col,new QTableWidgetItem(v[col]));
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);box->addWidget(table);
    auto close=new QDialogButtonBox(QDialogButtonBox::Close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);box->addWidget(close);dialog.exec();
}

void MainWindow::refreshInstalledKeys(){
    installedKeys.clear();
    if(hardwareActive){for(int i=0;i<samples.size();++i)if(samples[i].sampled)installedKeys.insert(i);}
    else for(const auto &k:visibleKeys)if(!k.decoration)installedKeys.insert(layout.index(k.matrix));
    expectedKeys=installedKeys.values();
}
void MainWindow::refreshCaptureView(){
    bool capture=firmwareCalibrationState==1||firmwareCalibrationState==3||firmwareCalibrationState==4;
    keyboard->setInstalled(installedKeys);keyboard->setCapturing(hardwareActive);matrix->setCalibration(hardwareActive,installedKeys);
    if(hardwareActive){auto x=summarizeCapture(samples,installedKeys);auto problem=QJsonDocument::fromJson(reviewData).object()["validationError"].toString();
        calibrationLabel->setText((!x.saved&&!x.captured?QString("Calibration required — "):QString{})+x.text()+(capture?QString(" — NEW captures are unsaved; Apply needs at least one valid new capture."):QString(" — saved state; no capture running."))+(problem.isEmpty()?QString{}:QString(" Saving blocked: ")+problem));progress->setRange(0,x.installed);progress->setValue(x.saved+x.captured);}
}
void MainWindow::queueActuation(const QString &name,const QByteArray &data){
    if(!hardwareActive||(currentDevice.ecMinor!=EC42_MINOR)||firmwareCalibrationState||vaState.size()!=20||!vaState[5])return;
    QString key=name;if(name=="vaSet")key+=":"+QString::number(wire::get16(data,0));
    vaError.clear();vaPending[key]=data;settingsStatus->setText("Pending USB update…");vaDebounce.start();
}
void MainWindow::receiveActuation(const QByteArray &data){
    if(data.size()!=20||quint8(data[4])>4||quint8(data[5])>1||quint8(data[13])>99){settingsStatus->setText("Error: invalid actuation readback");return;}
    if(data[4]==0&&data.left(2)!=data.mid(2,2)){settingsStatus->setText("Error: saved revision does not match active revision");return;}
    if(vaState.size()==20&&vaState.mid(11,2)!=data.mid(11,2))messages->appendPlainText(QString("RT movement now %1/%2 ADC counts (read back). Values outside the active window are normalized only after your requested setting change.").arg(quint8(data[11])).arg(quint8(data[12])));
    vaState=data;
    const QStringList states={"Saved to keyboard — driver readback verified; power-cycle test still required","Applied in RAM — waiting for idle EEPROM save","Saving to keyboard","Error — no automatic write retry; inspect/restore backup","Calibration required — empty RAM state; saved EEPROM was not overwritten"};
    settingsStatus->setText(states[quint8(data[4])]);
    if(!vaError.isEmpty())settingsStatus->setText("Error: "+vaError+" — release keys and retry explicitly");
    if(vaPending.isEmpty()&&!commandBusy&&!globalActuation->isSliderDown()&&!keyActuation->isSliderDown()&&!rtSensitivity->isSliderDown()){
        QSignalBlocker a(globalActuation),b(keyActuation),c(rtEnabled),d(rtSensitivity),f(rtPress),g(rtRelease);
        int percent=qRound(wire::get16(data,6)*100.0/1023);globalActuation->setValue(percent);globalActuation->setToolTip(QString("Global actuation: %1/1023 of calibrated ADC span. Endpoint 99 = Deepest (bottom minus 1 ADC); other steps are percentages.").arg(wire::get16(data,6)));
        if(wire::get16(data,18)==65535)vaSelected->setText(QString("Global default %1; select a calibrated key below to override it").arg(wire::get16(data,6)==1022?QString("Deepest (bottom minus 1 ADC)"):QString::number(percent)+"% ADC span"));
        if(wire::get16(data,18)==selected){keyActuation->setValue(data[13]?quint8(data[13]):percent);vaSelected->setText(QString("Selected (%1,%2): %3 — global %4/1023, override %5. Deepest stops one ADC count below measured bottom. Per-key overrides survive global changes.").arg(selected/layout.cols).arg(selected%layout.cols).arg(selected>=0&&selected<samples.size()&&samples[selected].calibrated?"calibrated":"inactive").arg(wire::get16(data,6)).arg(data[13]?(quint8(data[13])==99?QString("Deepest"):QString::number(quint8(data[13]))+"%"):"inherits"));}
        rtEnabled->setChecked(data[10]);rtPress->setValue(quint8(data[11]));rtRelease->setValue(quint8(data[12]));rtSensitivity->setValue(255-qMax(quint8(data[11]),quint8(data[12])));
        rtEffectiveStatus->setText(QString("Firmware RAM revision %1; verified saved revision %2. Dynamic RT %3; press/release movement %4/%5 ADC counts.").arg(wire::get16(data,0)).arg(wire::get16(data,2)).arg(data[10]?"ON":"OFF").arg(quint8(data[11])).arg(quint8(data[12])));
    }
    enableLiveControls();
}
} // namespace ec

