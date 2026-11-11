#pragma once

/* Alec's layout uses the shorter side clusters (both layout options zero).
 * Their six optional bottom switches are not installed. An unpopulated
 * beamspring pad must not be treated as a permanently depressed switch.
 * All five spacebar-row switches, including right Ctrl, remain enabled.
 * Standard vial retains the full 94-position source layout union.
 */
#undef LEYDEN_JAR_ACTIVE_PADS
#define LEYDEN_JAR_ACTIVE_PADS {0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf6, 0xfc, 0xfe, 0xfe, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
