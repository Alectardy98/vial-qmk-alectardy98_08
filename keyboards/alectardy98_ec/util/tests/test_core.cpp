// SPDX-License-Identifier: GPL-3.0-or-later
#include "layout.h"
#include "mainwindow.h"
#include "protocol.h"
#include "calibration.h"
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QtTest>
using namespace ec;
class Tests : public QObject {
    Q_OBJECT
    QString fixture(QString name = "reverseenginering.vial.json") const {
        return QString(EC_FIXTURE_DIR) + "/" + name;
    }
    QByteArray simple(QString keys = "\"0,0\"") const {
        return QString("{\"matrix\":{\"rows\":2,\"cols\":4},\"layouts\":{\"keymap\":[[%1]]}}")
            .arg(keys)
            .toUtf8();
    }
  private slots:
    void physicalCaptureColors(){
        auto l=LayoutParser::parse(simple("\"0,0\",\"0,1\""));KeyboardView v;v.setHardware(true);v.setCapturing(true);v.setInstalled({0});v.setKeys(l.visible({}),4);
        QVector<Sample> values(8);values[0].sampled=values[1].sampled=true;
        auto count=[&](QColor color){int n=0;for(auto item:v.QGraphicsView::scene()->items())if(auto path=dynamic_cast<QGraphicsPathItem*>(item))n+=path->brush().color()==color;return n;};
        v.setSamples(values,0,"Current");QCOMPARE(count(captureColor(0,false)),2);QCOMPARE(count(captureColor(0,true)),0);
        values[0].raw=1022;values[0].active=true;v.setSamples(values,0,"Current");QCOMPARE(count(captureColor(0,false)),2);
        values[0].captureStatus=1;v.setSamples(values,0,"Current");QCOMPARE(count(captureColor(1,false)),1);
        values[0].captureStatus=2;v.setSamples(values,0,"Current");QCOMPARE(count(captureColor(2,false)),1);
        values[0].captureStatus=1;v.setSamples(values,0,"Current");QCOMPARE(count(captureColor(1,false)),1);
    }
    void mainFixture() {
        auto l = LayoutParser::load(fixture());
        QCOMPARE(l.rows, 5);
        QCOMPARE(l.cols, 21);
        QCOMPARE(l.keys.size(), 100);
        QCOMPARE(l.options.size(), 5);
        auto m = l.mapped();
        QCOMPARE(m.size(), 105);
        QCOMPARE(std::count(m.begin(), m.end(), true), 91);
        QVERIFY(!m[1 * 21 + 16]);
        QVERIFY(l.warnings.join(' ').contains("Incomplete preset"));
    }
    void allOptions() {
        auto l = LayoutParser::load(fixture());
        QSet<int> covered;
        for (int bits = 0; bits < 32; ++bits) {
            QVector<int> choices;
            for (int i = 0; i < 5; ++i)
                choices << ((bits >> i) & 1);
            auto visible = l.visible(choices);
            QSet<int> unique;
            QRectF bounds;
            for (auto &k : visible) {
                QVERIFY(!unique.contains(l.index(k.matrix)));
                unique.insert(l.index(k.matrix));
                covered.insert(l.index(k.matrix));
                bounds = bounds.united(k.shape.boundingRect());
            }
            QVERIFY(visible.size() > 75);
            QVERIFY(bounds.width() < 25);
            QVERIFY(bounds.height() < 7);
        }
        QCOMPARE(covered.size(), 91);
    }
    void alternateAnchor() {
        auto l = LayoutParser::load(fixture("compact.vial.json"));
        auto wide = l.visible({0}), split = l.visible({1});
        QCOMPARE(wide.size(), 5);
        QCOMPARE(split.size(), 6);
        QCOMPARE(wide[1].shape.boundingRect().topLeft(), split[1].shape.boundingRect().topLeft());
        QCOMPARE(split[2].shape.boundingRect().left(), 2.0);
        QCOMPARE(l.presets["Split"], QVector<int>{1});
    }
    void invalidPresetRejected() {
        auto l = LayoutParser::parse(
            R"({"matrix":{"rows":1,"cols":2},"layouts":{"labels":["split"],"presets":{"Bad":[1]},"keymap":[["0,0\n\n\n0,0","0,1\n\n\n0,1","0,1\n\n\n0,1"]]}})");
        QVERIFY(l.presets.isEmpty());
        QVERIFY(l.warnings.join(' ').contains("Invalid preset"));
        QVERIFY_EXCEPTION_THROWN(l.visible({1}), std::runtime_error);
    }
    void nonrectangular() {
        auto l = LayoutParser::load(fixture());
        bool found = false;
        for (auto &k : l.keys)
            if (k.option == 1 && k.choice == 1 && k.matrix == Coordinate{2, 15}) {
                found = true;
                auto b = k.shape.boundingRect();
                QVERIFY(qAbs(b.width() - 1.5) < 1e-8);
                QVERIFY(qAbs(b.height() - 2) < 1e-8);
                QVERIFY(k.shape.contains(QPointF(b.left() + 0.1, b.top() + 0.1)));
                QVERIFY(!k.shape.contains(QPointF(b.left() + 0.1, b.bottom() - 0.1)));
            }
        QVERIFY(found);
    }
    void rotationAndReset() {
        auto l = LayoutParser::parse(simple("{\"r\":90,\"rx\":2,\"ry\":3,\"w\":2},\"0,0\",\"0,1\""));
        QCOMPARE(l.keys[0].shape.boundingRect(), QRectF(1, 3, 1, 2));
        QCOMPARE(l.keys[1].shape.boundingRect(), QRectF(1, 5, 1, 1));
    }
    void rotationClusterReset() {
        auto l = LayoutParser::parse(
            R"({"matrix":{"rows":2,"cols":2},"layouts":{"keymap":[[{"rx":2,"ry":3},"0,0"],[{"rx":5},"1,0"]]}})");
        QCOMPARE(l.keys[1].shape.boundingRect(), QRectF(5, 3, 1, 1));
    }
    void secondaryDimensionReset() {
        auto l = LayoutParser::parse(simple("{\"w2\":3},{\"w\":2},\"0,0\""));
        QCOMPARE(l.keys[0].shape.boundingRect(), QRectF(0, 0, 2, 1));
    }
    void rowSpacing() {
        auto l = LayoutParser::parse("{\"matrix\":{\"rows\":2,\"cols\":2},\"layouts\":{\"keymap\":[[{\"x\":0."
                                     "5,\"y\":0.25,\"w\":1.5},\"0,0\"],[\"1,0\"]]}}");
        QCOMPARE(l.keys[0].shape.boundingRect(), QRectF(.5, .25, 1.5, 1));
        QCOMPARE(l.keys[1].shape.boundingRect(), QRectF(0, 1.25, 1, 1));
    }
    void decorations() {
        auto l = LayoutParser::parse(simple("{\"d\":true},\"Blocker\",\"0,0\""));
        QVERIFY(l.keys[0].decoration);
        auto mapped = l.mapped();
        QCOMPARE(std::count(mapped.begin(), mapped.end(), true), 1);
    }
    void unsupportedReported() {
        auto l = LayoutParser::parse(simple("{\"mystery\":42},\"0,0\""));
        QVERIFY(l.warnings.join(' ').contains("mystery"));
    }
    void badJson_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("syntax") << QByteArray("{");
        QTest::newRow("root") << QByteArray("[]");
        QTest::newRow("missing") << QByteArray("{}");
        QTest::newRow("coordinate") << simple("\"banana\"");
        QTest::newRow("bounds") << simple("\"8,0\"");
        QTest::newRow("fractional dimension")
            << QByteArray("{\"matrix\":{\"rows\":1.5,\"cols\":2},\"layouts\":{\"keymap\":[[\"0,0\"]]}}");
        QTest::newRow("bad size") << simple("{\"w\":-1},\"0,0\"");
        QTest::newRow("duplicate") << simple("\"0,0\",\"0,0\"");
        QTest::newRow("bad option") << simple("\"0,0\\n\\n\\n0,1\"");
        QTest::newRow("bad rotation") << simple("\"0,0\",{\"r\":90},\"0,1\"");
        QTest::newRow("bad alignment") << simple("{\"a\":12},\"0,0\"");
    }
    void badJson() {
        QFETCH(QByteArray, json);
        QVERIFY_EXCEPTION_THROWN(LayoutParser::parse(json), std::runtime_error);
    }
    void packetGolden() {
        wire::Packet p{wire::Op::Hello, 0x1234, wire::Status::Ok, false, {}};
        auto b = wire::encode(p);
        QCOMPARE(b.size(), 32);
        QCOMPARE(b.left(12).toHex(), QByteArray("ca4543343202101234000000"));
        auto d = wire::decode(b);
        QCOMPARE(d.sequence, quint16(0x1234));
        QCOMPARE(d.op, wire::Op::Hello);
        QVERIFY(!d.response);
    }
    void packetValidation() {
        auto b = wire::encode({});
        for (int n : {0, 1, 31, 33})
            QVERIFY_EXCEPTION_THROWN(wire::decode(QByteArray(n, '\0')), std::runtime_error);
        for (auto change :
             QVector<QPair<int, int>>{{0, 0}, {4, '1'}, {5, 3}, {6, 0}, {9, 1}, {10, 21}, {11, 2}, {31, 1}}) {
            auto bad = b;
            bad[change.first] = char(change.second);
            QVERIFY_EXCEPTION_THROWN(wire::decode(bad), std::runtime_error);
        }
        wire::Packet huge;
        huge.payload = QByteArray(21, 'x');
        QVERIFY_EXCEPTION_THROWN(wire::encode(huge), std::runtime_error);
    }
    void packetRoundTrips() {
        for (int n = 0; n <= 20; ++n) {
            wire::Packet p{wire::Op::ReadPage, quint16(65000 + n), wire::Status::Ok, true,
                           QByteArray(n, char(n))};
            auto q = wire::decode(wire::encode(p));
            QCOMPARE(q.payload, p.payload);
            QCOMPARE(q.sequence, p.sequence);
        }
    }
    void mockErrors() {
        MockTransport mock(2, 4, QVector<bool>(8, true));
        QByteArray body;
        wire::put16(body, 1);
        wire::put16(body, 0);
        body.append(char(1));
        wire::Packet req{wire::Op::ReadPage, 1, wire::Status::Ok, false, body};
        QCOMPARE(wire::decode(mock.exchange(wire::encode(req))).status, wire::Status::Stale);
        req.op = wire::Op::Latch;
        req.payload = {};
        auto latched = wire::decode(mock.exchange(wire::encode(req)));
        quint16 id = wire::get16(latched.payload, 0);
        req.op = wire::Op::ReadPage;
        req.payload.clear();
        wire::put16(req.payload, id);
        wire::put16(req.payload, 7);
        req.payload.append(char(5));
        QCOMPARE(wire::decode(mock.exchange(wire::encode(req))).status, wire::Status::Range);
        req.payload = {};
        QCOMPARE(wire::decode(mock.exchange(wire::encode(req))).status, wire::Status::Malformed);
    }
    void snapshotsAreFrozen() {
        MockTransport mock(1, 1, {true});
        DeviceClient client(mock);
        client.hello();
        auto a = client.poll();
        mock.advance(1.5);
        wire::Packet req{wire::Op::ReadPage, 90, wire::Status::Ok, false, {}};
        wire::put16(req.payload, 1);
        wire::put16(req.payload, 0);
        req.payload.append(char(1));
        auto res = wire::decode(mock.exchange(wire::encode(req)));
        QCOMPARE(wire::get16(res.payload, 5), a[0].raw);
        auto b = client.poll();
        QVERIFY(a[0].raw != b[0].raw);
    }
    void mismatchedReply() {
        class Bad : public Transport {
            QByteArray exchange(const QByteArray &b) override {
                auto p = wire::decode(b);
                p.response = true;
                p.sequence++;
                return wire::encode(p);
            }
        } bad;
        DeviceClient client(bad);
        QVERIFY_EXCEPTION_THROWN(client.hello(), std::runtime_error);
    }
    void telemetry() {
        Simulator a(1, 2, {true, false}), b(1, 2, {true, false});
        bool active = false, released = false, was = false;
        for (int i = 0; i < 200; ++i) {
            double t = i * .05;
            a.step(t);
            b.step(t);
            auto s = a.samples()[0];
            QCOMPARE(s.raw, b.samples()[0].raw);
            QVERIFY(s.minimum <= s.raw);
            QVERIFY(s.maximum >= s.raw);
            QVERIFY(s.raw <= 1023);
            QVERIFY(s.release < s.press);
            QVERIFY(s.press < s.bottom);
            active |= s.active;
            released |= was && !s.active;
            was = s.active;
            QVERIFY(!a.samples()[1].sampled);
        }
        QVERIFY(active);
        QVERIFY(released);
        a.setSampleUnmapped(true, {true, false});
        a.step(10);
        QVERIFY(a.samples()[1].sampled);
        QVERIFY_EXCEPTION_THROWN(a.step(-1), std::runtime_error);
    }
    void completeProtocolMatrix() {
        auto l = LayoutParser::load(fixture());
        MockTransport mock(l.rows, l.cols, l.mapped());
        DeviceClient client(mock);
        QCOMPARE(client.hello(), qMakePair(5, 21));
        mock.advance(1);
        auto values = client.poll();
        QCOMPARE(values.size(), 105);
        QCOMPARE(std::count_if(values.begin(), values.end(), [](const auto &s) { return s.sampled; }), 91);
        QVERIFY(!values[37].sampled);
        mock.setSampleUnmapped(true, l.mapped());
        mock.advance(2);
        values = client.poll();
        QCOMPARE(std::count_if(values.begin(), values.end(), [](const auto &s) { return s.sampled; }), 105);
    }
    void boundedHistory() {
        History h;
        for (int i = 0; i < 1000; ++i) {
            Sample s;
            s.seconds = i;
            h.append(s);
        }
        QCOMPARE(h.values.size(), 600);
        QCOMPARE(h.values.first().seconds, 400.0);
    }
    void runtimeReloadAndSelection() {
        MainWindow w;
        QVERIFY(w.loadLayout(fixture()));
        QVERIFY(w.healthy());
        QCOMPARE(w.matrixCount(), 105);
        w.selectKey(1, 16);
        QVERIFY((w.selectedCoordinate() == Coordinate{1, 16}));
        w.setOption(0, 1);
        QVERIFY(w.healthy());
        QVERIFY(w.loadLayout(fixture("compact.vial.json")));
        QCOMPARE(w.matrixCount(), 8);
        QCOMPARE(w.physicalCount(), 5);
        w.setOption(0, 1);
        QCOMPARE(w.physicalCount(), 6);
        QVERIFY(!w.loadLayout("/no/such/file"));
        QCOMPARE(w.matrixCount(), 8);
        QVERIFY(w.healthy());
    }
    void allUiOptions() {
        MainWindow w;
        QVERIFY(w.loadLayout(fixture()));
        for (int bits = 0; bits < 32; ++bits) {
            for (int i = 0; i < 5; ++i)
                w.setOption(i, (bits >> i) & 1);
            w.tick();
            QVERIFY(w.healthy());
            QCOMPARE(w.matrixCount(), 105);
        }
    }
    void copiedFixtureIdentity() {
        QFile f(fixture());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex(),
                 QByteArray("38f61cbc047b8121b5afcafdefbcbd9028cffb94b417926b9a57bb6be18926d1"));
    }
    void keyClick() {
        KeyboardView view;
        auto l = LayoutParser::parse(simple());
        view.setKeys(l.visible({}), l.cols);
        view.resize(500, 220);
        view.show();
        QTest::qWait(20);
        QSignalSpy spy(&view, &KeyboardView::keySelected);
        auto center = view.mapFromScene(view.QGraphicsView::scene()->itemsBoundingRect().center());
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy[0][0].toInt(), 0);
        QCOMPARE(spy[0][1].toInt(), 0);
    }
};
QTEST_MAIN(Tests)
#include "test_core.moc"
