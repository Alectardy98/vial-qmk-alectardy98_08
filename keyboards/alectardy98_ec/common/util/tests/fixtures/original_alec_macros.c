/* Copyright 2022 Alectardy98
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Pre-optimization process_record_user reference, test fixture only.
 */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {             // Code for macros
        switch (keycode) {
            case TEST:
                SEND_STRING("TEST");
                break;
            case VDRT:
                SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_RIGHT) SS_UP(X_LCTL)));
                break;
            case VDLT:
                SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_LEFT) SS_UP(X_LCTL)));
                break;
            case VDUP:
                SEND_STRING(SS_LGUI(SS_TAP(X_TAB) SS_TAP(X_T)));
                break;
            case VDDN:
                SEND_STRING(SS_LGUI("d"));
                break;
            case EXPL:
                SEND_STRING(SS_LGUI("e"));
                break;
            case SNIP:
                SEND_STRING(SS_LGUI(SS_LSFT("s")));
                break;
            case COPY:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_C)));
                break;
            case CUTT:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_X)));
                break;
            case PAST:
                SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_V)));
                break;
            case TASK:
                SEND_STRING(SS_LCTL(SS_DOWN(X_RSFT) SS_TAP(X_ESCAPE) SS_UP(X_RSFT)));  // Windows
                SEND_STRING(SS_LCTL(SS_DOWN(X_LALT) SS_TAP(X_ESCAPE) SS_UP(X_LALT)));    // Mac
                break;
            case GOGL:
                SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_G))));
                break;
            case CMD:
                SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_T))));
                break;
            case QUIT:
                SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
                SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
                break;
            case DZRO:
                SEND_STRING("00");
                break;
            case ATAB:
                SEND_STRING(SS_LALT(SS_TAP(X_TAB)));
                break;
            case LPRC:
                SEND_STRING("(");
                break;
            case RPRC:
                SEND_STRING(")");
                break;
            case NUM:
                SEND_STRING(SS_TAP(X_NUM));         // Toggles layer change and presses the num lock button, to allow for Num lock toggle to occur for both Mac OS and Windows
                return true;
                break;
            // Discord Section
            case THIS:
                SEND_STRING(":this:" SS_TAP(X_ENTER));
                break;
            case OOF:
                SEND_STRING(":oof:" SS_TAP(X_ENTER));
                break;
            case PIKA:
                SEND_STRING(":surprised:" SS_TAP(X_ENTER));
                break;
            case SCAT:
                SEND_STRING(":smiley_cat2:" SS_TAP(X_ENTER));
                break;
            case FCAT:
                SEND_STRING(":smiley_cat1:" SS_TAP(X_ENTER));
                break;
            case HART:
                SEND_STRING(":heart:" SS_TAP(X_ENTER));
                break;
            case DROL:
                SEND_STRING(":drooling_face:" SS_TAP(X_ENTER));
                break;
            case MONY:
                SEND_STRING(":smiley_take:" SS_TAP(X_ENTER));
                break;
            case FHAT:
                SEND_STRING(":smiley_fedora:" SS_TAP(X_ENTER));
                break;
            case SPIT:
                SEND_STRING(":smiley_spit:" SS_TAP(X_ENTER));
                break;
            case KING:
                SEND_STRING(":pepe_king:" SS_TAP(X_ENTER));
                break;
            case FLEX:
                SEND_STRING(":muscle:" SS_TAP(X_ENTER));
                break;
            case HAHA:
                SEND_STRING(":smiley_kekw:" SS_TAP(X_ENTER));
                break;
            case LCRY:
                SEND_STRING(":joy:" SS_TAP(X_ENTER));
                break;
            case NICE:
                SEND_STRING(":nice~2:" SS_TAP(X_ENTER));
                break;
            case PPOG:
                SEND_STRING(":pepe_pog:" SS_TAP(X_ENTER));
                break;
            case PRAY:
                SEND_STRING(":pray_tone2:" SS_TAP(X_ENTER));
                break;
            case SWET:
                SEND_STRING(":sweat_smile:" SS_TAP(X_ENTER));
                break;
            case COLD:
                SEND_STRING(":cold_face:" SS_TAP(X_ENTER));
                break;
            case THUM:
                SEND_STRING(":thumbsup:" SS_TAP(X_ENTER));
                break;
        }
    }

#ifdef CONSOLE_ENABLE // Console Debug
    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %1u, time: %u, interrupt: %1u, count: %u\n",
            keycode, record->event.key.col, record->event.key.row, record->event.pressed, record->event.time,
            record->tap.interrupted, record->tap.count);
#endif

    return true;
}

