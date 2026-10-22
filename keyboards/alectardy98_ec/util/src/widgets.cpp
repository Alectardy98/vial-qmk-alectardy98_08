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
// Signal heatmap and current/min/max display adapted from Cap-Util signal_level.cpp.
// Copyright 2020 Purdea Andrei. Original licensed GPL-2.0-or-later.
// Adaptations and new code: Copyright 2026 QMK EC Utility contributors.
#include "widgets.h"
#include "calibration.h"
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QHeaderView>
#include <QPainter>
#include <functional>
namespace ec {
namespace {
QColor signalColor(quint16 raw, quint16 low, quint16 high) {
    // Cap-Util's red/green interpolation, bounded for flat/invalid spans.
    double scale = high > low ? qBound(0., (double(raw) - low) / (high - low), 1.) : 0.;
    return QColor(int(255 * (1 - scale) + 192 * scale), int(255 * scale + 192 * (1 - scale)), 192);
}
class KeyItem : public QGraphicsPathItem {
  public:
    Coordinate coordinate;
    int id = -1;
    bool decoration = false;
    QGraphicsSimpleTextItem *text;
    std::function<void()> chosen;
    KeyItem(const QPainterPath &path) : QGraphicsPathItem(path), text(new QGraphicsSimpleTextItem(this)) {
        text->setAcceptedMouseButtons(Qt::NoButton);
        text->setFont(QFont("Sans", 8));
        setAcceptedMouseButtons(Qt::LeftButton);
    }
    void centerText() {
        auto b = path().boundingRect();
        auto t = text->boundingRect();
        text->setPos(b.center() - QPointF(t.width() / 2, t.height() / 2));
    }

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
        if (!decoration && chosen)
            chosen();
        e->accept();
    }
};
} // namespace
KeyboardView::KeyboardView(QWidget *p) : QGraphicsView(p), scene(new QGraphicsScene(this)) {
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setMinimumHeight(230);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setBackgroundBrush(QColor("#20252d"));
}
void KeyboardView::setKeys(const QVector<VisibleKey> &keys, int cols) {
    columns = cols;
    scene->clear();
    QTransform scale;
    scale.scale(52, 52);
    for (auto &k : keys) {
        auto item = new KeyItem(scale.map(k.shape));
        item->coordinate = k.matrix;
        item->id = k.decoration ? -1 : k.matrix.row * columns + k.matrix.col;
        item->decoration = k.decoration;
        item->chosen = [this, c = k.matrix]() { emit keySelected(c.row, c.col); };
        item->setBrush(QColor("#e8e8e8"));
        item->setPen(QPen(Qt::black, 1));
        item->text->setText(k.decoration ? "" : QString("%1,%2").arg(k.matrix.row).arg(k.matrix.col));
        item->centerText();
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-5, -5, 5, 5));
    fit();
}
void KeyboardView::setSamples(const QVector<Sample> &samples, int selected, const QString &overlay) {
    for (auto base : scene->items()) {
        auto item = dynamic_cast<KeyItem *>(base);
        if (!item)
            continue;
        if (item->decoration) {
            item->setBrush(QColor("#747984"));
            continue;
        }
        if (item->id < 0 || item->id >= samples.size())
            continue;
        const auto &s = samples[item->id];
        QString value = "N/A";
        if (s.sampled) {
            if (overlay == "Baseline")
                value = QString::number(s.baseline);
            else if (overlay == "Bottom")
                value = s.calibrationMethod==2?QString("unmeasured"):QString::number(s.bottom);
            else if (overlay == "Press / release")
                value = QString("%1/%2").arg(s.press).arg(s.release);
            else if (overlay == "Min / max")
                value = QString("%1/%2").arg(s.minimum).arg(s.maximum);
            else if (overlay == "Press margin")
                value = QString::number(int(s.raw) - s.press);
            else
                value = QString::number(s.raw);
        }
        item->text->setText(QString("%1,%2\n%3%4")
                                .arg(item->coordinate.row)
                                .arg(item->coordinate.col)
                                .arg(value, s.active ? " *" : ""));
        if(capturing){
            bool excluded=!s.sampled;
            item->text->setText(QString("%1,%2\n%3").arg(item->coordinate.row).arg(item->coordinate.col).arg(excluded?QString("−"):s.captureStatus==1?QString("NEW"):s.captureStatus==2||s.captureStatus==3||(s.savedRetry||(s.savedQualified&&!s.calibrated))?QString("Retry"):s.calibrationMethod==3&&s.calibrated?QString("Saved"):QString("Inactive")));
            item->centerText();item->text->setBrush(excluded?QColor("#e5e7eb"):QColor(Qt::black));item->setBrush(captureFill(s));
            item->setPen(QPen(item->id==selected?QColor("#2467ef"):Qt::black,item->id==selected?3:1));
            item->setToolTip(captureLabel(s));
            continue;
        }
        item->centerText();item->text->setBrush(hardware?(!s.calibrated?QColor("#a8a8a8"):s.active?QColor("#32df77"):QColor("#ffd740")):QColor(Qt::black));
        item->setBrush(hardware ? QColor("#30343b")
                       : !s.sampled                            ? QColor("#b0b0b0")
                       : s.captured && hardware && capturing ? QColor("#88baff")
                       : s.active                            ? QColor("#7fd998")
                                                             : signalColor(s.raw, s.baseline, s.calibrationMethod==2?s.fixedPress:s.bottom));
        item->setPen(
            QPen(item->id == selected ? QColor("#2467ef") : Qt::black, item->id == selected ? 3 : 1));
        item->setToolTip(QString("Matrix %1,%2 — %3\n" + QString(hardware ? "REAL" : "SIMULATED") +
                                 "; logical=%4, debounced=%5")
                             .arg(item->coordinate.row)
                             .arg(item->coordinate.col)
                             .arg(s.sampled ? QString::number(s.raw) : "unsampled")
                             .arg(s.active)
                             .arg(s.debounced));
    }
}
void KeyboardView::fit() {
    if (!scene->sceneRect().isEmpty())
        fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}
void KeyboardView::resizeEvent(QResizeEvent *e) {
    QGraphicsView::resizeEvent(e);
    fit();
}
MatrixView::MatrixView(QWidget *p) : QTableWidget(p) {
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectItems);
    connect(this, &QTableWidget::cellClicked, this, [this](int r, int c) { emit keySelected(r, c); });
}
void MatrixView::setMatrix(const Layout &layout) {
    clear();
    setRowCount(layout.rows);
    setColumnCount(layout.cols);
    mapped = layout.mapped();
    for (int c = 0; c < layout.cols; ++c) {
        setHorizontalHeaderItem(c, new QTableWidgetItem(QString::number(c)));
        setColumnWidth(c, 64);
    }
    for (int r = 0; r < layout.rows; ++r) {
        setVerticalHeaderItem(r, new QTableWidgetItem(QString::number(r)));
        setRowHeight(r, 46);
        for (int c = 0; c < layout.cols; ++c)
            setItem(r, c, new QTableWidgetItem);
    }
}
void MatrixView::setSamples(const QVector<Sample> &samples) {
    for (int r = 0; r < rowCount(); ++r)
        for (int c = 0; c < columnCount(); ++c) {
            int i = r * columnCount() + c;
            if (i >= samples.size())
                continue;
            auto item = this->item(r, c);
            const auto &s = samples[i];
            item->setText((s.sampled ? QString::number(s.raw) : QString("N/A")) +
                          (mapped[i] ? "" : "\nUnmapped"));
            item->setBackground(!s.sampled ? QColor("#d7d7d7")
                                : s.active ? QColor("#80dc9a")
                                           : QColor("#edf2f9"));
            item->setForeground(Qt::black);
            if(capturing){bool excluded=!s.sampled;item->setBackground(captureFill(s));item->setForeground(excluded?QColor("#e5e7eb"):QColor(Qt::black));item->setText(excluded?"−":s.captureStatus==1?"NEW":s.captureStatus==2||s.captureStatus==3||(s.savedRetry||(s.savedQualified&&!s.calibrated))?"Retry":s.calibrationMethod==3&&s.calibrated?"Saved":"Inactive");item->setToolTip(captureLabel(s));continue;}
            item->setToolTip(QString("%1,%2 | %3 | %4\nLogical=%5; debounced=%6")
                                 .arg(r)
                                 .arg(c)
                                 .arg(mapped[i] ? "Mapped in layout union" : "No physical layout entry",
                                      s.sampled  ? hardware ? "Firmware cached sample" : "Simulated sample"
                                      : hardware ? "Not sampled by firmware"
                                                 : "Unsampled by simulation policy")
                                 .arg(s.active)
                                 .arg(s.debounced));
        }
}
void SignalGraph::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#17202b"));
    QRectF plot = QRectF(rect()).adjusted(48, 20, -16, -30);
    p.setPen(QColor("#c6d0dd"));
    p.drawText(8, 15, hardware ? "ADC counts — REAL cached readings" : "ADC counts — SIMULATED");
    for (int v : {0, 256, 512, 768, 1023}) {
        double y = plot.bottom() - v / 1023. * plot.height();
        p.setPen(QColor("#354454"));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(Qt::white);
        p.drawText(QRectF(0, y - 8, 42, 16), Qt::AlignRight, QString::number(v));
    }
    if (history.isEmpty()) {
        p.drawText(plot, Qt::AlignCenter, "Select a matrix position");
        return;
    }
    if (!history.last().sampled) {
        p.drawText(plot, Qt::AlignCenter, "Unsampled position — no signal history");
        return;
    }
    double end = history.last().seconds, start = qMax(0., end - 12.0);
    double span = qMax(1., end - start);
    auto draw = [&](QColor color, auto getter,bool bottomSeries=false) {
        QPainterPath line;
        bool open = false;
        for (const auto &s : history) {
            if (s.seconds < start || !s.sampled || (bottomSeries&&s.calibrationMethod==2)) {
                open = false;
                continue;
            }
            QPointF point(plot.left() + (s.seconds - start) / span * plot.width(),
                          plot.bottom() - getter(s) / 1023. * plot.height());
            if (!open) {
                line.moveTo(point);
                open = true;
            } else
                line.lineTo(point);
        }
        p.setPen(QPen(color, 1.5));
        p.drawPath(line);
    };
    p.save();
    p.setClipRect(plot);
    draw(QColor("#ffc857"), [](auto &s) { return s.baseline; });
    draw(QColor("#ed6a87"), [](auto &s) { return s.press; });
    draw(QColor("#b894f4"), [](auto &s) { return s.release; });
    draw(QColor("#72a5e8"), [](auto &s) { return s.bottom; },true);
    draw(QColor("#67e4bd"), [](auto &s) { return s.raw; });
    p.restore();
    p.setPen(Qt::white);
    p.drawText(
        QRectF(48, height() - 26, width() - 60, 22), Qt::AlignLeft,
        QString("%1–%2 s   raw: green | baseline: yellow | press: pink | release: violet | bottom: blue")
            .arg(start, 0, 'f', 1)
            .arg(end, 0, 'f', 1));
}
} // namespace ec
