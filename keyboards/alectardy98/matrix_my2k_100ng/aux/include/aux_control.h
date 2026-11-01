/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_CONTROL_H
#define AUX_CONTROL_H
#include "aux_stock_parity.h"
#include "aux_panels.h"
#include "my2k_wire.h"
#include "aux_text.h"
#if AUX_STOCK_PARITY
#    include "aux_clock.h"
#endif
#if MY2K_RELEASE1_PROFILE
#    include "aux_oled.h"
#endif
typedef struct {
    aux_panels *panels;
#if MY2K_RELEASE1_PROFILE
    aux_oled oled;
#endif
    bool     reply_pending, reply_color, target;
    aux_text text;
#if AUX_STOCK_PARITY
    aux_datetime clock;
    uint8_t      mono_mode;
    bool         clock_shown;
    bool         reply_asset;
    uint32_t     reload_completed, reload_errors, reload_output_failures;
    uint32_t     reply_deadline;
#endif
} aux_control;
/* SCREEN acknowledgements wait for the requested output transition. PWR is
 * deliberately unacknowledged, matching stock. Requests while an earlier SCR
 * response is pending fail explicitly; no unbounded command queue. */
bool aux_control_apply(aux_control *control, const my2k_wire_message *message, bool *reply);
/* 0=no completed response, 1=STS:OK, -1=STS:FAIL. Call after panel service. */
int aux_control_result(aux_control *control);
/* Worker-only, after text updates and before physical panel service. */
void aux_control_service(aux_control *, uint32_t now);
#endif
