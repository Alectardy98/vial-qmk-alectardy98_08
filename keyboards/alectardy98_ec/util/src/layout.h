// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QMap>
#include <QPainterPath>
#include <QPointF>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace ec {
struct Coordinate {
    int row = -1, col = -1;
    bool operator==(const Coordinate &b) const {
        return row == b.row && col == b.col;
    }
};
struct LayoutOption {
    QString name;
    QStringList choices;
};
struct PhysicalKey {
    Coordinate matrix;
    QPainterPath shape;
    int option = -1, choice = 0;
    bool decoration = false;
};
struct VisibleKey {
    int sourceIndex;
    Coordinate matrix;
    QPainterPath shape;
    bool decoration;
};
struct Layout {
    QString name;
    int rows = 0, cols = 0;
    QVector<PhysicalKey> keys;
    QVector<LayoutOption> options;
    QMap<QString, QVector<int>> presets;
    QStringList warnings;
    QVector<VisibleKey> visible(const QVector<int> &choices) const;
    QVector<bool> mapped() const;
    int index(Coordinate c) const {
        return c.row * cols + c.col;
    }
};
class LayoutParser {
  public:
    static Layout parse(const QByteArray &json);
    static Layout load(const QString &filename);
};
} // namespace ec
