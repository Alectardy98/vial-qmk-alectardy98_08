// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMetaType>
#include <QVector>
#include <limits>
namespace ec {
struct Sample {
    double seconds = 0;
    quint16 raw = 0, baseline = 0, bottom = 0, press = 0, release = 0;
    quint16 minimum = 0, maximum = 0, peak = 0, extremum = 0;
    quint8 mode = 0, calibrationMethod = 0, captureStatus = 0;
    quint16 fixedPress = 0, fixedRelease = 0;
    bool metadataKnown = false;
    double metadataSeconds = 0;
    bool savedQualified = false, savedRetry = false;
    bool sampled = false, calibrated = false, active = false, debounced = false, captured = false;
};
class Simulator {
  public:
    Simulator(int rows, int cols, QVector<bool> sampled);
    void step(double seconds);
    void setSampleUnmapped(bool enabled, const QVector<bool> &mapped);
    const QVector<Sample> &samples() const {
        return current;
    }
    int rows() const {
        return rowCount;
    }
    int cols() const {
        return colCount;
    }

  private:
    int rowCount, colCount;
    QVector<bool> sampled;
    QVector<Sample> current;
};
struct History {
    QVector<Sample> values;
    void append(const Sample &s) {
        values << s;
        if (values.size() > 600)
            values.remove(0, values.size() - 600);
    }
};
} // namespace ec

Q_DECLARE_METATYPE(QVector<ec::Sample>)
