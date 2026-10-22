// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protocol.h"
#include <QString>
namespace ec {
struct CalibrationRecord {QByteArray canonical; QVector<int> overrides; bool packed=false;};
CalibrationRecord decodeCalibrationRecord(const QByteArray &);
// Exact integer interpolation used by the current scanner.
int affineThreshold(int normalized, int baseline, int bottom);
QString calibrationSummary(const QVector<Sample> &samples, const QVector<int> &positions);
} // namespace ec
