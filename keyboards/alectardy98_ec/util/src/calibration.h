// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.h"
#include <QJsonObject>
#include <QColor>
#include <QSet>
namespace ec {
QJsonObject assessManualCapture(int baseline,int low,int high,int count,int flags,int pressQ,int releaseQ);
QString captureReason(int status);
QColor captureColor(int status,bool excluded);
QString captureLabel(const Sample &);
QColor captureFill(const Sample &);
struct CaptureSummary {int installed=0,captured=0,saved=0,retry=0,unmeasured=0,excluded=0;bool canApply()const{return captured>0;}QString text()const;};
CaptureSummary summarizeCapture(const QVector<Sample>&,const QSet<int>&);
}
