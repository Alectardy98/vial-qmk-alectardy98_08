// SPDX-License-Identifier: GPL-3.0-or-later
// Real shared C engine/board scanner subprocess; no HID interface is opened.
#include "device.h"
#include "settings.h"
#include "boardprofile.h"
#include <QProcess>
#include <QFile>
#include <QSignalSpy>
#include <QtTest>
#include <atomic>
#include <lzma.h>
using namespace ec;
namespace {
QByteArray load(const QString &board){QFile f(QString(EC_FIXTURE_DIR)+"/"+board+".vial.json");if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("layout fixture missing");return f.readAll();}
QByteArray compressed(const QByteArray &b){QByteArray out(lzma_stream_buffer_bound(b.size()),0);size_t n=0;if(lzma_easy_buffer_encode(6,LZMA_CHECK_CRC64,nullptr,reinterpret_cast<const uint8_t*>(b.data()),b.size(),reinterpret_cast<uint8_t*>(out.data()),&n,out.size())!=LZMA_OK)throw std::runtime_error("XZ failure");out.resize(n);return out;}
struct State{QString board,program;std::atomic<int> adc{100};std::atomic<bool> present{true};};
class Peer:public Transport {
 QProcess process;std::shared_ptr<State> state;QByteArray layout,uid;
public:
 explicit Peer(std::shared_ptr<State> s):state(s),layout(compressed(load(s->board))){
  for(const auto &p:boardProfiles())if(p.id==s->board)uid=p.uid;
  process.start(s->program);if(!process.waitForStarted())throw std::runtime_error("port harness missing");
 }
 ~Peer(){process.closeWriteChannel();if(!process.waitForFinished(1000)){process.kill();process.waitForFinished();}}
 QByteArray call(QByteArray q){if(process.write(q)!=32||!process.waitForBytesWritten(2000))throw std::runtime_error("harness write failed");QByteArray r;while(r.size()<32){if(!process.bytesAvailable()&&!process.waitForReadyRead(3000))throw std::runtime_error("harness response missing");r+=process.read(32-r.size());}return r;}
 void hold(int value){QByteArray q(32,0);q.replace(0,4,"TEST");q[4]=1;q[7]=char(value>>8);q[8]=char(value);call(q);q[4]=2;for(int i=0;i<40;i++)call(q);}
 QByteArray exchange(const QByteArray &q) override {
  if(!state->present)throw std::runtime_error("unplugged");QByteArray r(32,0);
  if(q[0]==1){r[0]=1;r[2]=12;return r;}
  if(quint8(q[0])==0xfe){
   if(q[1]==0){r[0]=6;r.replace(4,8,uid);return r;}
   if(q[1]==1){for(int i=0;i<4;i++)r[i]=char(layout.size()>>(8*i));return r;}
   if(q[1]==2){int page=quint8(q[2])+(quint8(q[3])<<8);r.replace(0,qMin(32,layout.size()-page*32),layout.mid(page*32,32));return r;}
  }
  if(q.left(2)==QByteArray::fromHex("0202")){r[0]=2;r[1]=2;return r;}
  hold(state->adc);return call(q);
 }
};
class Provider:public DeviceProvider {
 std::shared_ptr<State> state;
public:
 Provider(std::shared_ptr<State> s):state(s){}
 QVector<DeviceInfo> enumerate()override{if(!state->present)return {};DeviceInfo d;d.path="mock-port";d.name=state->board;return {d};}
 std::unique_ptr<Transport> open(const QByteArray &)override{return std::make_unique<Peer>(state);}
};
}
class Ports:public QObject {
 Q_OBJECT
private slots:
 void endToEnd_data(){QTest::addColumn<QString>("board");QTest::addColumn<QString>("program");QTest::addColumn<int>("cells");QTest::newRow("class0413")<<QString("class0413_ec")<<QString(EC_CLASS_HARNESS)<<105;QTest::newRow("prox")<<QString("ec_prox")<<QString(EC_PROX_HARNESS)<<75;QTest::newRow("matrix")<<QString("me_ec_uf2")<<QString(EC_MATRIX_HARNESS)<<96;}
 void endToEnd(){
  QFETCH(QString,board);QFETCH(QString,program);QFETCH(int,cells);
  auto state=std::make_shared<State>();state->board=board;state->program=program;
  Peer peer(state);auto info=VialReader::identify(peer,{});QCOMPARE(info.ecMinor,9);QCOMPARE(info.rows*info.cols,cells);
  auto layout=LayoutParser::parse(VialReader::definition(peer));QCOMPARE(layout.rows,info.rows);QCOMPARE(layout.cols,info.cols);
  DeviceClient client(peer);client.hello();QVERIFY(client.supportsExplicitCalibration());QCOMPARE(client.recordBytes(),17+3*cells);
  auto initial=readActive(client,info.rows,info.cols);QCOMPARE(decodeCalibrationRecord(initial).canonical.size(),9+2*cells);
  auto data=client.poll();QCOMPARE(data.size(),cells);QVERIFY(!data[0].active);QVERIFY(!data[0].calibrated);
  auto session=client.request(wire::Op::CalibrationControl,QByteArray::fromHex("0100")).payload;
  peer.hold(100);peer.hold(900);peer.hold(100);
  client.request(wire::Op::CalibrationControl,QByteArray(1,3));
  for(auto op:{wire::Op::CalibrationValidate,wire::Op::CalibrationActivate,wire::Op::CalibrationSave})client.request(op,session);
  auto saved=readBackup(client,info.rows,info.cols);validateBackup(saved,info.rows,info.cols);QVERIFY(decodeCalibrationRecord(saved).packed);QCOMPARE(decodeCalibrationRecord(saved).overrides.size(),cells);
  client.setCalibrationRecord(saved);client.calibrationStatus();data=client.poll();QVERIFY(data[0].calibrated);QVERIFY(!data[1].calibrated);QCOMPARE(data[0].bottom,quint16(900));
  for(int i=0;i<saved.size();i++){auto bad=saved;bad[i]^=1;QVERIFY_EXCEPTION_THROWN(validateBackup(bad,info.rows,info.cols),std::runtime_error);}
  restoreBackup(client,saved,info.rows,info.cols);QCOMPARE(readBackup(client,info.rows,info.cols),saved);
  QByteArray index;wire::put16(index,0);auto config=client.request(wire::Op::ActuationGet,index).payload;
  QByteArray set=index;set.append(char(99));set+=config.left(2);client.request(wire::Op::ActuationSet,set);
  QCOMPARE(decodeCalibrationRecord(readActive(client,info.rows,info.cols)).overrides[0],99);
  state->adc=899;data=client.poll(0,false,true);QCOMPARE(data[0].raw,quint16(899));QVERIFY(!data[0].active);
  state->adc=900;data=client.poll(0,false,true);QCOMPARE(data[0].raw,quint16(900));QVERIFY(data[0].active);QCOMPARE(data[0].fixedPress,quint16(899));
  state->adc=100;data=client.poll(0,false,true);QVERIFY(!data[0].active);QVERIFY(!data[1].calibrated);

 }
 void worker_data(){endToEnd_data();}
 void worker(){
  QFETCH(QString,board);QFETCH(QString,program);QFETCH(int,cells);Q_UNUSED(cells);
  auto state=std::make_shared<State>();state->board=board;state->program=program;
  SessionWorker worker(std::make_unique<Provider>(state));QSignalSpy ready(&worker,&SessionWorker::identified),done(&worker,&SessionWorker::completed),lost(&worker,&SessionWorker::connectionLost);
  worker.start();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),8000);auto info=qvariant_cast<DeviceInfo>(ready[0][0]);
  auto count=[&](const QString &name){int n=0;for(const auto &row:done)if(row[0].toString()==name)n++;return n;};
  worker.command(info.generation,"start");QTRY_COMPARE_WITH_TIMEOUT(count("start"),1,8000);
  state->adc=900;QTest::qWait(1000);state->adc=100;QTest::qWait(1000);
  worker.command(info.generation,"finish");QTRY_COMPARE_WITH_TIMEOUT(count("finish"),1,8000);
  QByteArray review;for(const auto &row:done)if(row[0].toString()=="finish")review=row[1].toByteArray();
  worker.command(info.generation,"applyManual",review);QTRY_COMPARE_WITH_TIMEOUT(count("applyManual"),1,8000);
  worker.command(info.generation,"vaSet",QByteArray::fromHex("ffff32"));QTRY_COMPARE_WITH_TIMEOUT(count("vaSet"),1,8000);
  worker.command(info.generation,"vaRt",QByteArray::fromHex("011010"));QTRY_COMPARE_WITH_TIMEOUT(count("vaRt"),1,8000);
  state->present=false;QTRY_VERIFY_WITH_TIMEOUT(!lost.isEmpty(),5000);worker.stop();QVERIFY(worker.wait(3000));
 }
};
QTEST_GUILESS_MAIN(Ports)
#include "test_ports_live.moc"
