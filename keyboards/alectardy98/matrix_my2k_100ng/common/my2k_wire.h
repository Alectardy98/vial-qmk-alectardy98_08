/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_WIRE_H
#define MY2K_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if MY2K_DEV22_PROFILE || MY2K_ACTIVE_DEV_PROFILE
#    define MY2K_DIAG_PAGES 29u
#elif MY2K_DEV21_PROFILE
#    define MY2K_DIAG_PAGES 10u
#elif MY2K_DEV7_PROFILE
#    define MY2K_DIAG_PAGES 6u
#else
#    define MY2K_DIAG_PAGES 4u
#endif
#define MY2K_UART_BAUD 115200u
#define MY2K_WIRE_MAX 128u
/* AC16 status byte6 retains bit0 and advertises atomic game handoff in bit1.
 * Only negotiated peers may send AC16 part2 instead of legacy part1. */
#define MY2K_GAME_ATOMIC_HANDOFF 2u
typedef enum {
    MY2K_SCR = 5,
    MY2K_LED = 6,
    MY2K_LYR = 7,
    MY2K_KH  = 8,
    MY2K_STS = 9,
    MY2K_TC  = 10,
    MY2K_TX  = 11,
    MY2K_TM  = 12,
    MY2K_TH  = 13,
    MY2K_PWR = 14
#if MY2K_DEV6_PROFILE
    ,
    MY2K_DQ = 15,
    MY2K_DR = 16
#endif

#if MY2K_RELEASE1_PROFILE
    ,
    MY2K_AC = 17,
    MY2K_AR = 18
#endif
} my2k_wire_type;
typedef enum {
    MY2K_SCREEN_PWR  = 0,
    MY2K_SCREEN_MSC  = 1,
    MY2K_SCREEN_MODE = 2,
    MY2K_SCREEN_ADJ  = 3,
    MY2K_SCREEN_DSP  = 4,
    MY2K_SCREEN_AM   = 5
} my2k_screen_field;
/* TC:v[0..7]; TM:year+v[0..5]=MH,DY,WY,HR,ME,SD; TH:v[0]=T,v[1]=H;
 * KH:v[0]=ROW,v[1]=COL,v[2]=PRESSED; SCR:v[0]=field,v[1]=value;
 * LED/LYR:v[0]; STS:v[0]=1(OK)/0(FAIL); PWR:v[0]=1(ON)/0(OFF).
 * TX:v[0..7] packed printable characters; all zero means OFF. No ACK.
 * Literal ';' is encoded as ';;' and '=' as '=;' within a TX frame.
 * PWR controls both displays and has no stock acknowledgement; SCR:PWR only
 * controls color and is acknowledged. No persistence implied. */
/* Release 1 AC/AR extensions use v[0] as operation, v[1] as request token,
 * and v[2] as an operation-specific argument. AR retains those fields and
 * puts completion/error status in v[3]. Payload layout and capability checks
 * belong to the receiving OLED, animation, game or sleep module. */
/* Dev6-only read-only extension: DQ:<page>; requests cached status;
 * DR:0=<page>;1=<byte>;...;7=<byte>; replies. Indexed numbers are hex.
 * Pages0..3, schema6 in page0 byte1. Never a manufacturer command. */
typedef struct {
    my2k_wire_type type;
    uint16_t       year;
    uint8_t        v[8];
} my2k_wire_message;
typedef struct {
    char   bytes[MY2K_WIRE_MAX];
    size_t used;
    bool   dropping;
} my2k_wire_parser;
/* Feed returns1 completed valid frame,0 incomplete,-1 discarded invalid/overflow frame.
 * Caller dispatches messages outside ISR. Unsupported prefixes are rejected. */
void my2k_wire_init(my2k_wire_parser *parser);
int  my2k_wire_feed(my2k_wire_parser *parser, uint8_t byte, my2k_wire_message *out);
bool my2k_wire_parse(const char *bytes, size_t size, my2k_wire_message *out);
/* Positive byte count includes newline, excludes optional terminating NUL.
 * Negative result means invalid message(-1) or insufficient capacity(-2). */
int my2k_wire_encode(const my2k_wire_message *message, char *out, size_t capacity);
#endif
