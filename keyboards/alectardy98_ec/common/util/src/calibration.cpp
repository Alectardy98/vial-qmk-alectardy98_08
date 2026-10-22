// SPDX-License-Identifier: GPL-3.0-or-later
#include "calibration.h"
#include <QStringList>
#include <stdexcept>
namespace ec {
QJsonObject assessManualCapture(int baseline,int low,int high,int count,int flags,int pressQ,int releaseQ) {
    QString reason;
    if(baseline<0||baseline>1023||low<0||low>1023||high<0||high>1023||count<0||count>255||pressQ<=0||pressQ>=1023||releaseQ<0||releaseQ>=pressQ) reason="invalid ADC or parameter range";
    else if(count<16) reason="unmeasured / insufficient scans";
    else if(low>high) reason="insufficient sustained measurement (qualified bounds overlap)";
    else if(high-low<100 || high<baseline+100) reason="unmeasured movement / insufficient travel (untouched key possible)";
    int press=baseline+pressQ*(high-baseline)/1023,release=baseline+releaseQ*(high-baseline)/1023;
    if(reason.isEmpty() && !(press>release && release>baseline && press<high)) reason="invalid derived thresholds";
    QJsonObject o{{"minimum",low},{"maximum",high},{"samplesLowerBound",count},{"railSeen",bool(flags&2)},
        {"valid",reason.isEmpty()},{"result",reason.isEmpty()?(high==1023?"captured; sustained ADC rail (travel lower bound)":"captured"):reason}};
    if(reason.isEmpty()){o["candidatePress"]=press;o["candidateRelease"]=release;}
    return o;
}

QString captureReason(int status){switch(status){
case 0:return "No firmware-qualified movement yet; untouched or absent keys are not failures.";
case 1:return "Firmware confirmed sustained movement and valid proposed thresholds (including RT limits).";
case 2:return "Insufficient sustained movement: release, then hold fully and release again.";
case 3:return "Invalid proposed thresholds (including release versus resting signal) or Rapid Trigger offsets. Retry or review Advanced settings.";
case 4:return "Firmware-defined nonexistent position; never requires calibration.";
default:return "Unknown firmware status; do not apply.";
}}
QColor captureColor(int status,bool excluded){if(excluded)return QColor("#454b55");if(status==1)return QColor("#88baff");if(status==2||status==3)return QColor("#ef7777");return QColor("#b0b0b0");}
QString captureLabel(const Sample &s){
    if(!s.sampled)return "Firmware nonexistent";
    if(s.captureStatus==1)return "NEW qualified capture — unsaved";
    if(s.captureStatus==2||s.captureStatus==3)return "Needs retry — disabled after Save. "+captureReason(s.captureStatus);
    if(s.calibrationMethod==3&&s.calibrated)return "Saved calibration — enabled; retained if untouched";
    if(s.savedRetry&&!s.savedQualified)return "Saved invalid attempt — disabled; no qualified bottom. Retry calibration.";
    if(s.savedQualified)return "Saved measurement retained — disabled; recalibrate or restore backup";
    return "Uncalibrated — inactive after partial Save";
}
QColor captureFill(const Sample &s){
    if(!s.sampled)return captureColor(0,true);
    if(s.captureStatus)return captureColor(s.captureStatus,false);
    if(s.calibrationMethod==3&&s.calibrated)return QColor("#477fc7");
    if(s.savedRetry||s.savedQualified)return captureColor(3,false);
    return captureColor(0,false);
}
QString CaptureSummary::text()const{return QString("%1 eligible | %2 saved enabled | %3 NEW unsaved | %4 retry | %5 uncalibrated | %6 nonexistent").arg(installed).arg(saved).arg(captured).arg(retry).arg(unmeasured).arg(excluded);}
CaptureSummary summarizeCapture(const QVector<Sample>&samples,const QSet<int>&){CaptureSummary x;for(const auto &s:samples){if(!s.sampled){++x.excluded;continue;}++x.installed;if(s.captureStatus==1)++x.captured;else if(s.captureStatus==2||s.captureStatus==3)++x.retry;else if(s.calibrationMethod==3&&s.calibrated)++x.saved;else if(s.savedRetry||s.savedQualified)++x.retry;else ++x.unmeasured;}return x;}

}
