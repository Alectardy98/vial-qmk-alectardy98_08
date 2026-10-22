// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "calibration.h"
#include <QtTest>
#include <QJsonArray>
using namespace ec;
class SettingsTests : public QObject {
    Q_OBJECT
    Sample sample(int n, int b) { Sample s; s.sampled = s.metadataKnown = true; s.baseline=n; s.bottom=b; return s; }
    QByteArray params(int mode, int p=70, int r=70) {
        QByteArray b(1,char(mode)); wire::put16(b,550); wire::put16(b,500);
        wire::put16(b,550); b.append(char(p)); b.append(char(r)); return b;
    }
private slots:
    void hardwareCalibrationPatterns() {
        auto untouched=assessManualCapture(216,275,280,255,0,550,500);
        QVERIFY(!untouched["valid"].toBool());QVERIFY(untouched["result"].toString().contains("untouched"));
        QVERIFY(assessManualCapture(216,275,800,255,0,550,500)["valid"].toBool());
        auto rail=assessManualCapture(216,275,1023,255,2,550,500);
        QVERIFY(rail["valid"].toBool());QVERIFY(rail["result"].toString().contains("lower bound"));
        QVERIFY(!assessManualCapture(216,1023,0,1,2,550,500)["valid"].toBool());
        QVERIFY(!assessManualCapture(216,275,65535,255,2,550,500)["valid"].toBool());
        // No movement, including a signal parked at a rail, cannot be a manual capture.
        QVERIFY(!assessManualCapture(216,1023,1023,255,2,550,500)["valid"].toBool());
    }

    void captureMapStates(){
        QVector<Sample> v(5);for(int i=0;i<4;++i)v[i].sampled=true;
        auto a=summarizeCapture(v,{});QCOMPARE(a.installed,4);QCOMPARE(a.unmeasured,4);QCOMPARE(a.excluded,1);QVERIFY(!a.canApply());
        v[0].raw=1022;v[0].active=true;QCOMPARE(captureFill(v[0]),QColor("#b0b0b0"));
        v[0].captureStatus=1;v[1].captureStatus=2;a=summarizeCapture(v,{});
        QCOMPARE(a.captured,1);QCOMPARE(a.retry,1);QCOMPARE(a.unmeasured,2);QVERIFY(a.canApply());
        QCOMPARE(captureFill(v[0]),QColor("#88baff"));QVERIFY(captureLabel(v[0]).contains("unsaved"));
        QCOMPARE(captureFill(v[1]),QColor("#ef7777"));QCOMPARE(captureFill(v[4]),QColor("#454b55"));
        v[0].captureStatus=0;v[0].calibrationMethod=3;v[0].calibrated=v[0].savedQualified=true;
        QCOMPARE(captureFill(v[0]),QColor("#477fc7"));QVERIFY(captureLabel(v[0]).contains("Saved calibration"));
        a=summarizeCapture(v,{});QCOMPARE(a.saved,1);QVERIFY(!a.canApply()); // saved alone is not a NEW capture
        v[0].calibrated=false;QCOMPARE(captureFill(v[0]),QColor("#ef7777"));
        v[0].savedQualified=false;v[0].savedRetry=true;QCOMPARE(captureFill(v[0]),QColor("#ef7777"));QVERIFY(captureLabel(v[0]).contains("no qualified bottom"));
    }



    void calibrationLabels() {
        QVector<Sample> s{sample(100,700),sample(100,1023),sample(100,150),Sample{}};
        s[0].calibrated=true;
        auto text=calibrationSummary(s,{0,1,2,3});
        QVERIFY(text.contains("1 span-valid")); QVERIFY(text.contains("1 fallback"));
        QVERIFY(text.contains("1 insufficient")); QVERIFY(text.contains("1 unsampled"));
    }


    void pacedPolling() {
        class Count : public Transport {
        public:
            MockTransport mock{5,21,QVector<bool>(105,true)}; int count=0;
            QByteArray exchange(const QByteArray &b) override {++count;return mock.exchange(b);}
        } t;
        DeviceClient c(t); c.hello();t.count=0;c.poll();QCOMPARE(t.count,127);
        t.count=0;auto v=c.poll(50,false);QVERIFY(t.count<=35);
        QVERIFY(v[50].metadataKnown); QCOMPARE(v[50].metadataSeconds,v[50].seconds);
        QCOMPARE(v.size(),105);
    }
};
QTEST_MAIN(SettingsTests)
#include "test_settings.moc"
