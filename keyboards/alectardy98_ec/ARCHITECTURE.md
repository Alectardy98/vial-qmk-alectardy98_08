# Canonical EC42 v2.9 architecture

README.md is the authoritative protocol/record/adapter reference. All three use
common/engine/calibration.c, record_storage.inc, actuation_control.inc and
common/runtime/ec_board.c/decision.c. No active radix codec remains.

The canonical record is 17+3*N bytes (332/242/305); descriptor format 1 and persisted
method 0x42. Firmware and GUI share common/interfaces/ec42_protocol.h. Acquisition,
physical storage guards, native features and bootloaders remain board-specific.

0413 preserves Vial addresses with a219+113 split allocation and a113-byte macro
capacity reduction. See README section5 before installation: oversized old macros
block EC saves instead of being erased. Prox/Matrix stored bytes are unchanged.
Old calibration is not migrated; keep pre-flash backups and fallback firmware.
