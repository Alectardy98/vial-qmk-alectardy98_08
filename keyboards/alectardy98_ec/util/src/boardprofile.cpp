// SPDX-License-Identifier: GPL-3.0-or-later
#include "boardprofile.h"
namespace ec {
const QVector<BoardProfile> &boardProfiles() {
    static const QVector<BoardProfile> profiles={
        {"class0413_ec","MM Studio Class0413 EC","class0413_ec.vial.json",QByteArray::fromHex("45464c4449414702"),5,21,true,0xa105},
        {"ec_prox","Cipulot EC Pro X ANSI/ISO","ec_prox.vial.json",QByteArray::fromHex("4d2d00bcadf8b927"),5,15,true,0xa975},
        {"me_ec_uf2","Matrix Astro ME EC","me_ec_uf2.vial.json",QByteArray::fromHex("0ec173239f8f8f2c"),6,16,true,0xa996}
    };
    return profiles;
}
const BoardProfile *boardProfile(const QByteArray &uid) {
    for(const auto &profile:boardProfiles())if(profile.uid==uid)return &profile;
    return nullptr;
}
}
