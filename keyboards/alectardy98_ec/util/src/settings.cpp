// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "device.h"
#include "boardprofile.h"
#include <stdexcept>
namespace ec {
CalibrationRecord decodeCalibrationRecord(const QByteArray &wireRecord) {
    CalibrationRecord result{wireRecord,{},false};
    // EC42 v2.9 has a separate matrix-sized record, with metadata outside cells.
    // Geometry defines record length; the connected descriptor binds identity.
    if(wireRecord.size()>=20&&(wireRecord.size()-17)%3==0){
        int cells=(wireRecord.size()-17)/3,meta=9+2*cells,end=meta+8;
        quint16 magic=wire::get16(wireRecord,meta),method=wire::get16(wireRecord,meta+2);
        if(!magic||magic==65535||(method!=3&&method!=0x42))throw std::runtime_error("Portable EC record identity/version invalid");
        QByteArray native=wireRecord;
        for(int i=1;i<7;i+=2)std::swap(native[i],native[i+1]);
        for(int i=9;i<end;i+=2)std::swap(native[i],native[i+1]);
        quint32 expected=wire::get16(wireRecord,meta+4)|(quint32(wire::get16(wireRecord,meta+6))<<16);
        for(int i=meta+4;i<meta+8;i++)native[i]=0;
        if(configCrc(native)!=expected)throw std::runtime_error("Portable EC record CRC invalid");
        if(quint8(wireRecord[0])>1||!wire::get16(wireRecord,1)||wire::get16(wireRecord,1)>=1023||!wire::get16(wireRecord,3)||wire::get16(wireRecord,3)>=wire::get16(wireRecord,1)||!quint8(wireRecord[7])||!quint8(wireRecord[8]))throw std::runtime_error("Portable EC parameters invalid");
        result.overrides=QVector<int>(cells);
        for(int i=0;i<cells;i++){
            int v=wire::get16(wireRecord,9+2*i),override=quint8(wireRecord[end+i]);
            if(override>99||(v&0x1c00)||((v&0x4000)&&!(v&1023))||((v&0x8000)&&(!(v&0x4000)||(v&0x2000)))||(!(v&0x4000)&&(v&~0x2000)))throw std::runtime_error("Portable EC cell invalid");
            result.overrides[i]=override;
        }
        result.canonical=wireRecord.left(meta);result.packed=method==0x42;return result;
    }

    if(wireRecord.isEmpty())return result; // No active record loaded yet.
    throw std::runtime_error("EC42 v2.9 explicit record required");
}

int affineThreshold(int x, int n, int b) {
    if (x < 0 || x > 1023 || n < 0 || b > 1023 || b < n)
        throw std::runtime_error("Invalid affine calibration range");
    return n + x * (b - n) / 1023;
}
QString calibrationSummary(const QVector<Sample> &s, const QVector<int> &positions) {
    int valid = 0, fallback = 0, invalid = 0, unsampled = 0;
    for (int i : positions) {
        if (i < 0 || i >= s.size() || !s[i].sampled) { ++unsampled; continue; }
        if (s[i].bottom == 1023) ++fallback;
        else if (s[i].calibrated && s[i].bottom >= s[i].baseline + 100) ++valid;
        else ++invalid;
    }
    return QString("Installed layout: %1 span-valid | %2 fallback/saturated (1023) | %3 insufficient | %4 unsampled. "
                   "Span-valid is a firmware heuristic, not proof of full travel or physical EEPROM persistence.")
        .arg(valid).arg(fallback).arg(invalid).arg(unsampled);
}
} // namespace ec
