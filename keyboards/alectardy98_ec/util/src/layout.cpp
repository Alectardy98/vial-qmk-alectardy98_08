// SPDX-License-Identifier: GPL-3.0-or-later
#include "layout.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTransform>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ec {
namespace {
[[noreturn]] void fail(const QString &s) {
    throw std::runtime_error(s.toStdString());
}
int integer(const QJsonValue &v, const QString &what, int low, int high) {
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != std::floor(v.toDouble()) ||
        v.toDouble() < low || v.toDouble() > high)
        fail(what + ": invalid integer");
    return v.toInt();
}
double number(const QJsonObject &o, const QString &k, double fallback) {
    if (!o.contains(k))
        return fallback;
    auto v = o[k];
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || std::abs(v.toDouble()) > 1000)
        fail("Invalid KLE numeric property: " + k);
    return v.toDouble();
}
// KLE serialized legend index -> normalized physical legend position.
const int labelMap[8][12] = {
    {0, 6, 2, 8, 9, 11, 3, 5, 1, 4, 7, 10},        {1, 7, -1, -1, 9, 11, 4, -1, -1, -1, -1, 10},
    {3, -1, 5, -1, 9, 11, -1, -1, 4, -1, -1, 10},  {4, -1, -1, -1, 9, 11, -1, -1, -1, -1, -1, 10},
    {0, 6, 2, 8, 10, -1, 3, 5, 1, 4, 7, -1},       {1, 7, -1, -1, 10, -1, 4, -1, -1, -1, -1, -1},
    {3, -1, 5, -1, 10, -1, -1, -1, 4, -1, -1, -1}, {4, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1}};
QString labelAt(const QStringList &labels, int alignment, int normalized) {
    for (int i = 0; i < labels.size() && i < 12; ++i)
        if (labelMap[alignment][i] == normalized)
            return labels[i];
    return {};
}
QPair<int, int> pair(const QString &s, const QString &what) {
    static const QRegularExpression re("^\\s*(\\d+)\\s*,\\s*(\\d+)\\s*$");
    auto m = re.match(s);
    if (!m.hasMatch())
        fail(what + ": expected row,column or option,choice: " + s);
    bool a, b;
    int x = m.captured(1).toInt(&a), y = m.captured(2).toInt(&b);
    if (!a || !b)
        fail(what + ": integer overflow");
    return {x, y};
}
} // namespace
Layout LayoutParser::load(const QString &filename) {
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
        fail(f.errorString());
    if (f.size() > 4 * 1024 * 1024)
        fail("JSON exceeds 4 MiB limit");
    return parse(f.readAll());
}
Layout LayoutParser::parse(const QByteArray &data) {
    if (data.size() > 4 * 1024 * 1024)
        fail("JSON exceeds 4 MiB limit");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        fail(QString("JSON byte %1: %2").arg(error.offset).arg(error.errorString()));
    if (!doc.isObject())
        fail("Expected Vial JSON object");
    auto obj = doc.object();
    Layout out;
    out.name = obj.value("name").toString("Unnamed keyboard");
    auto matrix = obj.value("matrix").toObject();
    out.rows = integer(matrix["rows"], "matrix.rows", 1, 255);
    out.cols = integer(matrix["cols"], "matrix.cols", 1, 255);
    if (out.rows * out.cols > 4096)
        fail("Matrix exceeds 4096-position limit");
    auto layouts = obj.value("layouts").toObject();
    if (!layouts.value("keymap").isArray())
        fail("Missing layouts.keymap array");
    if (layouts.contains("labels") && !layouts["labels"].isArray())
        fail("layouts.labels must be an array");
    for (auto v : layouts["labels"].toArray()) {
        LayoutOption o;
        if (v.isString()) {
            o.name = v.toString();
            o.choices = {"Off", "On"};
        } else if (v.isArray()) {
            auto a = v.toArray();
            if (a.size() < 3)
                fail("Choice option needs caption and two choices");
            for (auto s : a)
                if (!s.isString())
                    fail("Option labels must be strings");
            o.name = a[0].toString();
            for (int i = 1; i < a.size(); ++i)
                o.choices << a[i].toString();
        } else
            fail("Invalid layout option");
        out.options << o;
        if (out.options.size() > 32 || o.choices.size() > 64)
            fail("Too many layout options or choices");
    }
    double x = 0, y = 0, rx = 0, ry = 0, angle = 0, w = 1, h = 1, x2 = 0, y2 = 0, w2 = 0, h2 = 0;
    bool decoration = false;
    int alignment = 4;
    bool first = true;
    const QSet<QString> numeric = {"x", "y", "w", "h", "x2", "y2", "w2", "h2", "r", "rx", "ry"};
    const QSet<QString> cosmetic = {"c", "t", "f", "f2", "fa", "p", "n", "l", "g", "sm", "sb", "st"};
    for (auto rowValue : layouts["keymap"].toArray()) {
        if (rowValue.isObject() && first) {
            out.warnings << "KLE metadata ignored";
            first = false;
            continue;
        }
        first = false;
        if (!rowValue.isArray())
            fail("KLE row must be an array");
        auto row = rowValue.toArray();
        for (int itemIndex = 0; itemIndex < row.size(); ++itemIndex) {
            auto item = row[itemIndex];
            if (item.isObject()) {
                auto p = item.toObject();
                for (auto it = p.begin(); it != p.end(); ++it)
                    if (!numeric.contains(it.key()) && !cosmetic.contains(it.key()) && it.key() != "a" &&
                        it.key() != "d")
                        out.warnings << "Unsupported KLE property: " + it.key();
                if (itemIndex != 0 && (p.contains("r") || p.contains("rx") || p.contains("ry")))
                    fail("KLE rotation must begin a row");
                angle = number(p, "r", angle);
                if (p.contains("rx")) {
                    rx = number(p, "rx", rx);
                    x = rx;
                    y = ry;
                }
                if (p.contains("ry")) {
                    ry = number(p, "ry", ry);
                    x = rx;
                    y = ry;
                }
                x += number(p, "x", 0);
                y += number(p, "y", 0);
                w = number(p, "w", w);
                h = number(p, "h", h);
                if (p.contains("w"))
                    w2 = w;
                if (p.contains("h"))
                    h2 = h;
                x2 = number(p, "x2", x2);
                y2 = number(p, "y2", y2);
                w2 = number(p, "w2", w2);
                h2 = number(p, "h2", h2);
                if (p.contains("a"))
                    alignment = integer(p["a"], "KLE alignment", 0, 7);
                if (p.contains("d")) {
                    if (!p["d"].isBool())
                        fail("KLE d must be boolean");
                    decoration = p["d"].toBool();
                }
                continue;
            }
            if (!item.isString())
                fail("KLE item must be property object or string");
            if (w <= 0 || h <= 0 || w2 < 0 || h2 < 0)
                fail("Key geometry has invalid dimensions");
            PhysicalKey key;
            key.decoration = decoration;
            auto labels = item.toString().split('\n');
            auto coord = labelAt(labels, alignment, 0);
            auto opt = labelAt(labels, alignment, 8);
            if (!decoration) {
                if (coord.isEmpty())
                    fail("Key has no top-left matrix coordinate (check KLE alignment)");
                auto c = pair(coord, "Matrix coordinate");
                key.matrix = {c.first, c.second};
                if (c.first >= out.rows || c.second >= out.cols)
                    fail("Matrix coordinate outside dimensions: " + coord);
            }
            if (!opt.isEmpty()) {
                auto o = pair(opt, "Layout option");
                key.option = o.first;
                key.choice = o.second;
                if (key.option >= out.options.size() || key.choice >= out.options[key.option].choices.size())
                    fail("Layout option index/choice out of range");
            }
            QPainterPath shape;
            shape.addRect(QRectF(x, y, w, h));
            if (w2 > 0 || h2 > 0 || x2 != 0 || y2 != 0) {
                QPainterPath secondary;
                secondary.addRect(QRectF(x + x2, y + y2, w2 > 0 ? w2 : w, h2 > 0 ? h2 : h));
                shape = shape.united(secondary);
            }
            QTransform t;
            t.translate(rx, ry);
            t.rotate(angle);
            t.translate(-rx, -ry);
            key.shape = t.map(shape);
            out.keys << key;
            if (out.keys.size() > 8192)
                fail("Too many physical keys");
            x += w;
            w = h = 1;
            x2 = y2 = w2 = h2 = 0;
            decoration = false;
        }
        y += 1;
        x = rx;
    }
    if (out.keys.isEmpty())
        fail("No physical keys");
    auto presets = layouts.value("presets");
    if (!presets.isUndefined() && !presets.isObject())
        fail("layouts.presets must be an object");
    auto presetObj = presets.toObject();
    for (auto it = presetObj.begin(); it != presetObj.end(); ++it) {
        if (!it->isArray()) {
            out.warnings << "Invalid preset: " + it.key();
            continue;
        }
        auto a = it->toArray();
        if (a.size() != out.options.size()) {
            out.warnings << "Incomplete preset ignored: " + it.key();
            continue;
        }
        QVector<int> choices;
        bool valid = true;
        for (int i = 0; i < a.size(); ++i) {
            if (!a[i].isDouble() || a[i].toDouble() != std::floor(a[i].toDouble()) || a[i].toInt() < 0 ||
                a[i].toInt() >= out.options[i].choices.size()) {
                valid = false;
                break;
            }
            choices << a[i].toInt();
        }
        if (valid)
            out.presets[it.key()] = choices;
        else
            out.warnings << "Invalid preset ignored: " + it.key();
    }
    out.warnings.removeDuplicates();
    out.visible(
        QVector<int>(out.options.size(), 0)); // validate default option anchors and duplicate coordinates
    for (auto it = out.presets.begin(); it != out.presets.end();) {
        try {
            out.visible(it.value());
            ++it;
        } catch (const std::exception &e) {
            out.warnings << QString("Invalid preset ignored: %1 (%2)").arg(it.key(), e.what());
            it = out.presets.erase(it);
        }
    }
    return out;
}
QVector<bool> Layout::mapped() const {
    QVector<bool> m(rows * cols, false);
    for (auto &k : keys)
        if (!k.decoration)
            m[index(k.matrix)] = true;
    return m;
}
QVector<VisibleKey> Layout::visible(const QVector<int> &choices) const {
    if (choices.size() != options.size())
        fail("Layout option count mismatch");
    for (int i = 0; i < choices.size(); ++i)
        if (choices[i] < 0 || choices[i] >= options[i].choices.size())
            fail("Layout choice out of range");
    QMap<QPair<int, int>, QPointF> anchors;
    for (auto &k : keys)
        if (k.option >= 0) {
            auto id = qMakePair(k.option, k.choice);
            auto p = k.shape.boundingRect().topLeft();
            if (!anchors.contains(id))
                anchors[id] = p;
            else
                anchors[id] = QPointF(qMin(anchors[id].x(), p.x()), qMin(anchors[id].y(), p.y()));
        }
    QVector<VisibleKey> result;
    QSet<int> used;
    for (int i = 0; i < keys.size(); ++i) {
        auto &k = keys[i];
        QPointF offset;
        if (k.option >= 0) {
            if (choices[k.option] != k.choice)
                continue;
            auto def = qMakePair(k.option, 0);
            if (!anchors.contains(def))
                fail("Alternative has no default option anchor");
            offset = anchors[def] - anchors[qMakePair(k.option, k.choice)];
        }
        if (!k.decoration) {
            int id = index(k.matrix);
            if (used.contains(id))
                fail("Duplicate coordinate in selected layout");
            used.insert(id);
        }
        QTransform t;
        t.translate(offset.x(), offset.y());
        result << VisibleKey{i, k.matrix, t.map(k.shape), k.decoration};
    }
    return result;
}
} // namespace ec
