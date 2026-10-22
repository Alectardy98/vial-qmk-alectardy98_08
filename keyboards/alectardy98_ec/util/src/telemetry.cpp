// SPDX-License-Identifier: GPL-3.0-or-later
#include "telemetry.h"
#include <cmath>
#include <stdexcept>
namespace ec {
Simulator::Simulator(int rows, int cols, QVector<bool> mask)
    : rowCount(rows), colCount(cols), sampled(std::move(mask)), current(rows * cols) {
    if (rows < 1 || cols < 1 || rows * cols > 4096 || sampled.size() != rows * cols)
        throw std::runtime_error("Invalid simulated matrix");
}
void Simulator::setSampleUnmapped(bool enabled, const QVector<bool> &mapped) {
    if (mapped.size() != sampled.size())
        throw std::runtime_error("Mask size mismatch");
    for (int i = 0; i < sampled.size(); ++i)
        sampled[i] = enabled || mapped[i];
}
void Simulator::step(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::runtime_error("Invalid simulation time");
    for (int i = 0; i < current.size(); ++i) {
        auto &s = current[i];
        bool previousSample = s.sampled;
        bool previousActive = s.active;
        s.seconds = seconds;
        s.sampled = sampled[i];
        s.calibrated = s.sampled;
        if (!s.sampled) {
            s.raw = s.minimum = s.maximum = 0;
            s.active = s.debounced = false;
            continue;
        }
        s.baseline = 90 + (i * 7) % 60;
        s.bottom = 780 + (i * 11) % 120;
        s.press = s.baseline + 550 * (s.bottom - s.baseline) / 1023;
        s.release = s.baseline + 500 * (s.bottom - s.baseline) / 1023;
        double phase = std::fmod(seconds + i * 0.19, 8.0);
        double travel = phase < 3 ? std::pow(std::sin(phase * 3.141592653589793 / 3), 2) : 0;
        int reading =
            std::lround(s.baseline + travel * (s.bottom - s.baseline) + 3 * std::sin(seconds * 19 + i));
        s.raw = qBound(0, reading, 1023);
        if (!previousSample) {
            s.minimum = s.maximum = s.raw;
            s.active = false;
        } else {
            s.minimum = qMin(s.minimum, s.raw);
            s.maximum = qMax(s.maximum, s.raw);
        }
        if (s.active && s.raw < s.release)
            s.active = false;
        else if (!s.active && s.raw > s.press)
            s.active = true;
        // Deliberate one-frame simulated debounce; not a claim about QMK timing.
        s.debounced = previousSample ? previousActive : false;
    }
}
} // namespace ec
