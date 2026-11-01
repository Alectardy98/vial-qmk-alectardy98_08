/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_wire.h"
static bool eq(const char *p, size_t n, const char *s) {
    size_t i = 0;
    while (s[i]) {
        if (i >= n || p[i] != s[i]) return false;
        i++;
    }
    return i == n;
}
static bool number(const char *p, size_t n, unsigned base, uint32_t max, uint32_t *out) {
    uint32_t v = 0;
    if (!n) return false;
    for (size_t i = 0; i < n; i++) {
        unsigned c = (unsigned char)p[i], d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return false;
        if (d >= base || v > (max - d) / base || d > max) return false;
        v = v * base + d;
    }
    *out = v;
    return true;
}
static bool valid(const my2k_wire_message *m) {
#if MY2K_RELEASE1_PROFILE
    if(m->type==MY2K_AC||m->type==MY2K_AR)return true;
#endif
#if MY2K_DEV6_PROFILE
    if(m->type==MY2K_DQ || m->type==MY2K_DR)return m->v[0]<MY2K_DIAG_PAGES;
#endif
    if (m->type == MY2K_TX) {
        bool ended = false;
        for (unsigned i = 0; i < 8; i++) {
            unsigned c = m->v[i];
            if (!c) ended = true;
            else if (ended || c < 32 || c > 126) return false;
        }
        return true;
    }
    if (m->type == MY2K_TM) {
        unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m->year < 1 || m->v[0] < 1 || m->v[0] > 12) return false;
        if (m->v[0] == 2 && m->year % 4 == 0 && (m->year % 100 != 0 || m->year % 400 == 0)) days[1] = 29;
        return m->v[1] >= 1 && m->v[1] <= days[m->v[0] - 1] && m->v[2] >= 1 && m->v[2] <= 7 && m->v[3] < 24 && m->v[4] < 60 && m->v[5] < 60;
    }
    if (m->type == MY2K_KH) return m->v[2] <= 1;
    if (m->type == MY2K_STS || m->type == MY2K_PWR) return m->v[0] <= 1;
    if (m->type == MY2K_SCR) return m->v[0] <= MY2K_SCREEN_AM && (m->v[0] > MY2K_SCREEN_MSC || m->v[1] <= 1);
    return m->type == MY2K_TC || m->type == MY2K_TH || m->type == MY2K_LED || m->type == MY2K_LYR;
}
bool my2k_wire_parse(const char *p, size_t n, my2k_wire_message *out) {
    if (!p || !out || n < 4 || n >= MY2K_WIRE_MAX || p[n - 1] != '\n') return false;
    my2k_wire_message m     = {0};
    size_t            colon = 0;
    while (colon < n && p[colon] != ':')
        colon++;
    if (colon == n) return false;
    if (eq(p, colon, "TC"))
        m.type = MY2K_TC;
    else if (eq(p, colon, "TX"))
        m.type = MY2K_TX;
    else if (eq(p, colon, "TM"))
        m.type = MY2K_TM;
    else if (eq(p, colon, "TH"))
        m.type = MY2K_TH;
    else if (eq(p, colon, "KH"))
        m.type = MY2K_KH;
    else if (eq(p, colon, "LED"))
        m.type = MY2K_LED;
    else if (eq(p, colon, "LYR"))
        m.type = MY2K_LYR;
    else if (eq(p, colon, "SCR"))
        m.type = MY2K_SCR;
    else if (eq(p, colon, "STS"))
        m.type = MY2K_STS;
    else if (eq(p, colon, "PWR"))
        m.type = MY2K_PWR;
#if MY2K_DEV6_PROFILE
    else if(eq(p,colon,"DQ"))m.type=MY2K_DQ;
    else if(eq(p,colon,"DR"))m.type=MY2K_DR;
#endif
#if MY2K_RELEASE1_PROFILE
    else if(eq(p,colon,"AC"))m.type=MY2K_AC;
    else if(eq(p,colon,"AR"))m.type=MY2K_AR;
#endif
    else
        return false;
    unsigned seen = 0, fields = 0;
    size_t   pos = colon + 1;
    while (pos < n - 1) {
        size_t end = pos;
        while (end < n - 1 && p[end] != ';')
            end++;
        /* TX's serializer emits the semicolon character followed by its
         * separator. Stock parser 080047fc consumes this doubled delimiter. */
        if (m.type == MY2K_TX && end == pos && end + 1 < n - 1 && p[end + 1] == ';') end++;
        if (end == n - 1 || end == pos) return false;
        size_t split = pos;
        while (split < end && p[split] != '=')
            split++;
        bool     pair  = split < end;
        uint32_t value = 0, key = 0;
        unsigned slot = 0;
        if (m.type == MY2K_TX) {
            /* Stock takes each token's first byte and OFF clears all slots.
             * Accept its canonical one-character/OFF forms, never silently
             * truncate oversized tokens or a ninth character. */
            if (fields >= 8) return false;
            if (eq(p + pos, end - pos, "OFF")) {
                if (fields || end + 1 != n - 1) return false;
            } else {
                if (end - pos != 1) return false;
                m.v[fields] = (uint8_t)p[pos];
            }
            slot = fields;
        } else if (m.type == MY2K_TC
#if MY2K_RELEASE1_PROFILE
                   || m.type==MY2K_AC || m.type==MY2K_AR
#endif
#if MY2K_DEV6_PROFILE
                   || m.type==MY2K_DR
#endif
        ) {
            if (!pair || !number(p + pos, split - pos, 16, 7, &key) || !number(p + split + 1, end - split - 1, 16, 255, &value)) return false;
            slot      = (unsigned)key;
            m.v[slot] = (uint8_t)value;
        } else if (m.type == MY2K_TM || m.type == MY2K_TH || m.type == MY2K_KH) {
            if (!pair) return false;
            const char *const  tm[]  = {"YR", "MH", "DY", "WY", "HR", "ME", "SD"};
            const char *const  th[]  = {"T", "H"};
            const char *const  kh[]  = {"ROW", "COL", "PRESSED"};
            const char *const *keys  = m.type == MY2K_TM ? tm : m.type == MY2K_TH ? th : kh;
            unsigned           count = m.type == MY2K_TM ? 7 : m.type == MY2K_TH ? 2 : 3;
            while (slot < count && !eq(p + pos, split - pos, keys[slot]))
                slot++;
            if (slot == count || !number(p + split + 1, end - split - 1, 10, m.type == MY2K_TM && slot == 0 ? 65535 : 255, &value)) return false;
            if (m.type == MY2K_TM) {
                if (!slot)
                    m.year = (uint16_t)value;
                else
                    m.v[slot - 1] = (uint8_t)value;
            } else
                m.v[slot] = (uint8_t)value;
        } else if (m.type == MY2K_SCR) {
            const char *const keys[] = {"PWR", "MSC", "MODE", "ADJ", "DSP", "AM"};
            if (!pair) return false;
            while (slot < 6 && !eq(p + pos, split - pos, keys[slot]))
                slot++;
            if (slot == 6 || fields) return false;
            m.v[0] = (uint8_t)slot;
            if (slot < 2) {
                if (eq(p + split + 1, end - split - 1, "ON"))
                    value = 1;
                else if (eq(p + split + 1, end - split - 1, "OFF"))
                    value = 0;
                else
                    return false;
            } else if (!number(p + split + 1, end - split - 1, 10, 255, &value))
                return false;
            m.v[1] = (uint8_t)value;
            slot   = 0;
        } else {
            if (pair || fields) return false;
            if (m.type == MY2K_STS || m.type == MY2K_PWR) {
                if (eq(p + pos, end - pos, m.type == MY2K_PWR ? "ON" : "OK"))
                    value = 1;
                else if (eq(p + pos, end - pos, m.type == MY2K_PWR ? "OFF" : "FAIL"))
                    value = 0;
                else
                    return false;
            } else if (!number(p + pos, end - pos, 10, 255, &value))
                return false;
            m.v[0] = (uint8_t)value;
        }
        if (seen & (1u << slot)) {
            return false;
        }
        seen |= 1u << slot;
        fields++;
        pos = end + 1;
    }
    unsigned wanted = m.type == MY2K_TX ? ((1u << fields) - 1) : (m.type == MY2K_TC
#if MY2K_RELEASE1_PROFILE
                   || m.type==MY2K_AC || m.type==MY2K_AR
#endif
#if MY2K_DEV6_PROFILE
        ||m.type==MY2K_DR
#endif
        ) ? 255 : m.type == MY2K_TM ? 127 : m.type == MY2K_TH ? 3 : m.type == MY2K_KH ? 7 : 1;
    if (!fields) return false;
    if (seen != wanted || !valid(&m)) return false;
    *out = m;
    return true;
}
void my2k_wire_init(my2k_wire_parser *p) {
    if (p) {
        p->used     = 0;
        p->dropping = false;
    }
}
int my2k_wire_feed(my2k_wire_parser *p, uint8_t byte, my2k_wire_message *out) {
    if (!p || !out) return -1;
    if (p->dropping) {
        if (byte == '\n') {
            my2k_wire_init(p);
            return -1;
        }
        return 0;
    }
    if (byte == 0 || p->used >= MY2K_WIRE_MAX - 1) {
        p->dropping = true;
        p->used     = 0;
        if (byte == '\n') {
            my2k_wire_init(p);
            return -1;
        }
        return 0;
    }
    p->bytes[p->used++] = (char)byte;
    if (byte != '\n') return 0;
    bool ok = my2k_wire_parse(p->bytes, p->used, out);
    my2k_wire_init(p);
    return ok ? 1 : -1;
}
typedef struct {
    char  *p;
    size_t cap, n;
    bool   failed;
} writer;
static void put(writer *w, char c) {
    if (w->n >= w->cap) {
        w->failed = true;
        return;
    }
    w->p[w->n++] = c;
}
static void str(writer *w, const char *s) {
    while (*s)
        put(w, *s++);
}
static void num(writer *w, uint32_t v, unsigned base) {
    char     b[10];
    unsigned n = 0;
    do {
        b[n++] = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);
    while (n)
        put(w, b[--n]);
}
int my2k_wire_encode(const my2k_wire_message *m, char *out, size_t cap) {
    if (!m || !out || !valid(m)) return -1;
    writer w = {out, cap, 0, false};
    switch (m->type) {
        case MY2K_TX:
            str(&w, "TX:");
            if (!m->v[0]) str(&w, "OFF;");
            else for (unsigned i = 0; i < 8 && m->v[i]; i++) {
                put(&w, (char)m->v[i]);
                put(&w, ';');
            }
            break;
#if MY2K_RELEASE1_PROFILE
        case MY2K_AC:
        case MY2K_AR:
#endif
        case MY2K_TC:
#if MY2K_DEV6_PROFILE
        case MY2K_DR:
            #if MY2K_RELEASE1_PROFILE
            str(&w,m->type==MY2K_AC?"AC:":m->type==MY2K_AR?"AR:":m->type==MY2K_DR?"DR:":"TC:");
#else
            str(&w,m->type==MY2K_DR?"DR:":"TC:");
#endif
#else
            str(&w,"TC:");
#endif
            for (unsigned i = 0; i < 8; i++) {
                num(&w, i, 16);
                put(&w, '=');
                num(&w, m->v[i], 16);
                put(&w, ';');
            }
            break;
        case MY2K_TM: {
            const char *keys[] = {"YR", "MH", "DY", "WY", "HR", "ME", "SD"};
            str(&w, "TM:");
            for (unsigned i = 0; i < 7; i++) {
                str(&w, keys[i]);
                put(&w, '=');
                num(&w, i ? m->v[i - 1] : m->year, 10);
                put(&w, ';');
            }
            break;
        }
        case MY2K_TH:
        case MY2K_KH: {
            const char        *th[] = {"T", "H"}, *kh[] = {"ROW", "COL", "PRESSED"};
            const char *const *keys = m->type == MY2K_TH ? th : kh;
            str(&w, m->type == MY2K_TH ? "TH:" : "KH:");
            for (unsigned i = 0; i < (m->type == MY2K_TH ? 2u : 3u); i++) {
                str(&w, keys[i]);
                put(&w, '=');
                num(&w, m->v[i], 10);
                put(&w, ';');
            }
            break;
        }
#if MY2K_DEV6_PROFILE
        case MY2K_DQ:
            str(&w,"DQ:");num(&w,m->v[0],10);put(&w,';');break;
#endif
        case MY2K_LED:
        case MY2K_LYR:
            str(&w, m->type == MY2K_LED ? "LED:" : "LYR:");
            num(&w, m->v[0], 10);
            put(&w, ';');
            break;
        case MY2K_STS:
            str(&w, m->v[0] ? "STS:OK;" : "STS:FAIL;");
            break;
        case MY2K_PWR:
            str(&w, m->v[0] ? "PWR:ON;" : "PWR:OFF;");
            break;
        case MY2K_SCR: {
            const char *keys[] = {"PWR", "MSC", "MODE", "ADJ", "DSP", "AM"};
            str(&w, "SCR:");
            str(&w, keys[m->v[0]]);
            put(&w, '=');
            if (m->v[0] < 2)
                str(&w, m->v[1] ? "ON" : "OFF");
            else
                num(&w, m->v[1], 10);
            put(&w, ';');
            break;
        }
        default:
            return -1;
    }
    put(&w, '\n');
    if (w.failed) return -2;
    if (w.n < cap) out[w.n] = '\0';
    return (int)w.n;
}
