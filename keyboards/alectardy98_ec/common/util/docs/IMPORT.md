# GUI provenance and canonical v2.9 update

The initial application was imported from the preserved external qmk-ec-util
repository. IMPORTED_SOURCE_SHA256.json and the original hashes below describe
that import, not the current canonical source. The current GUI has a generic
v2.9 descriptor/record codec, shared wire definitions and all-three-board tests;
current cleanup changes and tests are in development/alectardy98_ec/architecture_review/CLEANUP_REPORT.md.
The external repository and fallback executables remain unchanged.

Historical import record follows:

# Universal GUI import

Imported from /home/alec/Documents/GitHub/qmk-ec-util. The universal source tree
and build-universal executable were selected over the older build/ executable:
the source contains all three enabled board profiles and portable v2.9 codecs,
and its universal 14-test normal/sanitizer suites passed in the preceding audit.
The imported tree was independently rebuilt and tested. This is source/test
validation; physical tests are the owner's separately reported results.

IMPORTED_SOURCE_SHA256.json records the original source hashes. All runtime
code is identical except displayed messages/version formatting in livepanel.cpp/device.cpp/protocol.cpp: generic
EC storage/native bootloader replaces misleading sector-2/TinyUF2/test-firmware
labels; connected version uses the existing identified ecMinor value. No command encoding, driver, handler or algorithm was changed. CMake/test
path defaults and documentation were also adjusted. No caches,
repository history or old binaries were copied. The newly built Linux binary
is in build-universal/qmk-ec-util and is dynamically linked to system libraries.
The external repository and both fallback executables were left unchanged.

Original universal binary SHA-256: `1837dd41a13fc177f910cf49c93d3fe21db0464a45b99011837bc97297ca4395`.
Older build/ binary SHA-256: `36d34005d12e09a4d9d125522b698e4cf983deacd8a5c5db1baaab016cbf999c`.

Known-good framework/source/firmware and GUI binaries were backed up before
cleanup under /home/alec/firmware-analysis/analysis/ec42_housekeeping/before.tar.gz.
That snapshot and before-sha256.json are retained independently of the GUI.
