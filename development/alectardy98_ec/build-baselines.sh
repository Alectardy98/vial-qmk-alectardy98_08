#!/bin/sh
# Software builds only; no device access. Back up release artifacts first.
set -eu
cd "$(dirname "$0")/../.."
make clean
for board in class0413_ec ec_prox me_ec_uf2; do
 for key in vial vial_alec; do make "alectardy98_ec/$board:$key"; done
done
python3 development/alectardy98_ec/verify-builds.py .
