// SPDX-License-Identifier: GPL-3.0-or-later
#include "boardprofile.h"
#include "layout.h"
#include "device.h"
#include "settings.h"
#include <stdexcept>
#include <QtTest>
class Boards:public QObject {
 Q_OBJECT
private slots:
 void unknownBoardCanonicalDescriptor(){
  class Peer:public ec::Transport {
   public: int calls=0;
   QByteArray exchange(const QByteArray &q)override{
    ++calls;QByteArray b(32,0);
    if(quint8(q[0])==1){b[0]=1;b[2]=12;return b;}
    if(q.left(2)==QByteArray::fromHex("fe00")){b[0]=6;b.replace(4,8,QByteArray::fromHex("0123456789abcdef"));return b;}
    auto p=ec::wire::decode(q);QByteArray out;
    if(p.op==ec::wire::Op::Hello)out=QByteArray::fromHex("51454332090002000303ff0100660501");
    else if(p.op==ec::wire::Op::Descriptor)out=QByteArray::fromHex("77aa002303ff0001000040000000800001000101");
    else if(p.op==ec::wire::Op::FirmwareInfo)out="fourth-board";
    else throw std::runtime_error("Unexpected write");
    return ec::wire::encode({p.op,p.sequence,ec::wire::Status::Ok,true,out});
   }
  } peer;
  auto info=ec::VialReader::identify(peer,{});QCOMPARE(info.rows,2);QCOMPARE(info.cols,3);QCOMPARE(info.ecMinor,9);QCOMPARE(peer.calls,5);
  // A sixth-cell canonical record needs no registry entry or model-specific codec.
  QByteArray b(35,0);auto put=[&](int o,quint16 v){b[o]=char(v>>8);b[o+1]=char(v);};
  put(1,550);put(3,500);put(5,550);b[7]=16;b[8]=16;put(9,0xc384);put(21,0x77aa);put(23,0x42);
  QByteArray native=b;for(int o:{1,3,5,9,11,13,15,17,19,21,23,25,27})std::swap(native[o],native[o+1]);auto crc=ec::configCrc(native);put(25,crc);put(27,crc>>16);
  auto decoded=ec::decodeCalibrationRecord(b);QVERIFY(decoded.packed);QCOMPARE(decoded.overrides.size(),6);ec::validateBackup(b,2,3);
  auto bad=b;bad[9]^=1;QVERIFY_EXCEPTION_THROWN(ec::decodeCalibrationRecord(bad),std::runtime_error);
  ec::DeviceClient client(peer);client.hello();int before=peer.calls;
  // Valid record for a different descriptor magic must be rejected before RestoreBegin.
  auto foreign=b;foreign[21]=0x66;native=foreign;for(int o:{1,3,5,9,11,13,15,17,19,21,23,25,27})std::swap(native[o],native[o+1]);for(int i=25;i<29;i++)native[i]=0;crc=ec::configCrc(native);foreign[25]=char(crc>>8);foreign[26]=char(crc);foreign[27]=char(crc>>24);foreign[28]=char(crc>>16);
  QVERIFY_EXCEPTION_THROWN(ec::restoreBackup(client,foreign,2,3),std::runtime_error);QCOMPARE(peer.calls,before);
 }
 void unsupportedEcVersionsNeverWrite(){
  class Peer:public ec::Transport {
   public:int minor=8,calls=0;
   QByteArray exchange(const QByteArray &b)override{
    ++calls;auto q=ec::wire::decode(b);
    if(q.op!=ec::wire::Op::Hello)throw std::runtime_error("Command after unsupported version");
    auto payload=QByteArray::fromHex("51454332090005001503ff0100660501");payload[4]=char(minor);
    return ec::wire::encode({q.op,q.sequence,ec::wire::Status::Ok,true,payload});
   }
  };
  for(int version:{0,1,2,3,4,5,6,7,8,10}){Peer p;p.minor=version;ec::DeviceClient c(p);QVERIFY_EXCEPTION_THROWN(c.hello(),std::runtime_error);QCOMPARE(p.calls,1);}
  QVERIFY_EXCEPTION_THROWN(ec::decodeCalibrationRecord(QByteArray(219,0)),std::runtime_error);
 }
 void unsupportedNeverWrites(){
  class Probe:public ec::Transport {
   public:QByteArray uid;int calls=0;
   QByteArray exchange(const QByteArray &request) override {
    calls++;QByteArray reply(32,0);
    if(calls==1){if(quint8(request[0])!=1)throw std::runtime_error("unexpected command");reply[0]=1;reply[2]=12;return reply;}
    if(calls==2){if(request.left(2)!=QByteArray::fromHex("fe00"))throw std::runtime_error("unexpected command");reply[0]=6;reply.replace(4,8,uid);return reply;}
    if(calls==3&&quint8(request[6])==0x10)return reply; // old firmware: no valid capability response
    throw std::runtime_error("Write sent to unsupported firmware");
   }
  };
  for(const auto &p:ec::boardProfiles())if(p.id!="class0413_ec"){
   Probe probe;probe.uid=p.uid;bool rejected=false;
   try {ec::VialReader::identify(probe,{});}catch(const std::runtime_error &e){(void)e;rejected=true;}
   QVERIFY(rejected);QCOMPARE(probe.calls,3);
  }
 }
 void competingVialReply(){
  class CompetingPeer:public ec::Transport {
   public:int calls=0;
   QByteArray exchange(const QByteArray &request)override{
    ++calls;
    if(calls==1)return QByteArray::fromHex("0100090000000000000000000000000000000000000000000000000000000000");
    if(calls==2&&request.left(2)==QByteArray::fromHex("fe00"))
     return QByteArray::fromHex("0100ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    throw std::runtime_error("Unexpected command after invalid identity");
   }
  } peer;
  bool rejected=false;
  try{ec::VialReader::identify(peer,{});}catch(const std::runtime_error &e){
   rejected=true;QVERIFY(QString::fromUtf8(e.what()).contains("close Vial"));
  }
  QVERIFY(rejected);QCOMPARE(peer.calls,2); // no EC or calibration/write request
 }
 void unhealthyStorageDoesNotBlockInspection(){
  class Peer:public ec::Transport {
   public:int calls=0;
   QByteArray exchange(const QByteArray &b)override{
    ++calls;auto q=ec::wire::decode(b);QByteArray payload;
    if(q.op==ec::wire::Op::Hello)payload=QByteArray::fromHex("51454332090005000f03ff0100660501");
    else if(q.op==ec::wire::Op::Descriptor)payload=QByteArray::fromHex("a97500f203ff0001000040000000800002000001");
    else throw std::runtime_error("Unexpected write");
    return ec::wire::encode({q.op,q.sequence,ec::wire::Status::Ok,true,payload});
   }
  } peer;
  ec::DeviceClient client(peer);QCOMPARE(client.hello(),qMakePair(5,15));
  QVERIFY(!client.storageReady());QCOMPARE(peer.calls,2);
 }
 void physicalProxInterface(){
  QVERIFY(ec::HidProvider::isVialCandidate(0xff60,0x61,"vial:f64c2b3c"));
  QVERIFY(!ec::HidProvider::isVialCandidate(0xff31,0x74,"vial:f64c2b3c"));
  QVERIFY(!ec::HidProvider::isVialCandidate(1,6,"vial:f64c2b3c"));
 }
 void profiles(){
  const auto &profiles=ec::boardProfiles();QCOMPARE(profiles.size(),3);
  for(const auto &p:profiles){
   QCOMPARE(ec::boardProfile(p.uid),&p);
   const auto layout=ec::LayoutParser::load(QStringLiteral(EC_FIXTURE_DIR)+"/"+p.layout);
   QCOMPARE(layout.rows,p.rows);QCOMPARE(layout.cols,p.cols);
   QVERIFY(!layout.keys.isEmpty());
   for(const auto &key:layout.keys)if(!key.decoration){QVERIFY(key.matrix.row>=0&&key.matrix.row<p.rows);QVERIFY(key.matrix.col>=0&&key.matrix.col<p.cols);}
   QVERIFY(p.explicitCalibrationAvailable);
  }
  QVERIFY(!ec::boardProfile(QByteArray(8,0)));
 }
};
QTEST_GUILESS_MAIN(Boards)
#include "test_boards.moc"
