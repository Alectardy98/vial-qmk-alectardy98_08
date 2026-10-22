// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "layout.h"
#include "telemetry.h"
#include <QGraphicsView>
#include <QTableWidget>
#include <QWidget>
#include <QSet>
namespace ec {
class KeyboardView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit KeyboardView(QWidget *parent = nullptr);
    void setHardware(bool enabled) {
        hardware = enabled;
    }
    void setCapturing(bool enabled) {
        capturing = enabled;
    }
    void setInstalled(const QSet<int> &keys){installed=keys;}
    void setKeys(const QVector<VisibleKey> &keys, int columns);
    void setSamples(const QVector<Sample> &samples, int selected, const QString &overlay);
  signals:
    void keySelected(int row, int col);

  protected:
    void resizeEvent(QResizeEvent *) override;

  private:
    QGraphicsScene *scene;
    int columns = 0;
    bool hardware = false, capturing = false;
    QSet<int> installed;
    void fit();
};
class MatrixView : public QTableWidget {
    Q_OBJECT
  public:
    explicit MatrixView(QWidget *parent = nullptr);
    void setHardware(bool enabled) {
        hardware = enabled;
    }
    void setCalibration(bool enabled,const QSet<int>&keys){capturing=enabled;installed=keys;}
    void setMatrix(const Layout &layout);
    void setSamples(const QVector<Sample> &samples);
  signals:
    void keySelected(int row, int col);

  private:
    QVector<bool> mapped;
    bool hardware = false,capturing=false;
    QSet<int> installed;
};
class SignalGraph : public QWidget {
    Q_OBJECT
  public:
    explicit SignalGraph(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(190);
        setMinimumWidth(400);
    }
    void setHardware(bool enabled) {
        hardware = enabled;
        update();
    }
    void setHistory(const QVector<Sample> &h) {
        history = h;
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override;

  private:
    QVector<Sample> history;
    bool hardware = false;
};
} // namespace ec
