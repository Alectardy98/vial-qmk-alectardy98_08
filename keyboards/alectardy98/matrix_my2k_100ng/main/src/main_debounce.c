/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "debounce.h"
#include "timer.h"
#include "main_settings.h"
#if MY2K_DEV16_PROFILE
#include "main_input_performance.h"
#include "main_perf.h"
#endif
#include <string.h>
static matrix_row_t candidate[MATRIX_ROWS];
static uint32_t changed_at[MATRIX_ROWS][MATRIX_COLS];
void debounce_init(uint8_t rows){(void)rows;memset(candidate,0,sizeof candidate);memset(changed_at,0,sizeof changed_at);
#if MY2K_DEV16_PROFILE
my2k_input_reset();
#endif
}
void debounce_free(void){}
bool debounce(matrix_row_t raw[],matrix_row_t cooked[],uint8_t rows,bool changed){
    (void)changed;if(rows>MATRIX_ROWS)rows=MATRIX_ROWS;
    bool any=false;uint32_t now=timer_read32();
    for(unsigned r=0;r<rows;r++)for(unsigned c=0;c<MATRIX_COLS;c++){
        matrix_row_t bit=(matrix_row_t)1<<c;bool value=(raw[r]&bit)!=0;
        if(value!=((candidate[r]&bit)!=0)){candidate[r]^=bit;changed_at[r][c]=now;
#if MY2K_DEV16_PROFILE
my2k_input_raw_edge(r,c,value,my2k_perf_clock());
#endif
}
        if(value!=((cooked[r]&bit)!=0) && (uint32_t)(now-changed_at[r][c])>=my2k_main_debounce(value)){cooked[r]^=bit;any=true;
#if MY2K_DEV16_PROFILE
my2k_input_cooked_edge(r,c,value,my2k_perf_clock());
#endif
}
    }
    return any;
}
