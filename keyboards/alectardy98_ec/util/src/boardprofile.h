// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
namespace ec {
struct BoardProfile {
    QString id, name, layout;
    QByteArray uid;
    int rows, cols;
    // Identification does not authorize writes. Only a validated EC42
    // handshake and an implemented persistent-record codec can do that.
    bool explicitCalibrationAvailable;
    quint16 calibrationMagic=0; // expected explicit-record magic; descriptor remains authoritative
};
const QVector<BoardProfile> &boardProfiles();
const BoardProfile *boardProfile(const QByteArray &vialUid);
}
