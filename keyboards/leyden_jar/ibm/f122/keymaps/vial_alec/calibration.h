#pragma once

/* Personal Rev 3 F122, measured with every key released:
 * startup median 379, two pads at 390, previous +10 trigger 389.
 * +16 restores five counts of released margin on that capture. It does not
 * change matrix wiring or add scan passes. Held-key/chord margins still need
 * physical validation; GUI calibration preferences can override this default.
 */
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {16}
