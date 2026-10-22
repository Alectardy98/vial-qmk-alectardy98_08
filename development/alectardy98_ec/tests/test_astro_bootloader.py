#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute the actual Matrix QMK hook against mocked volatile F411 registers."""
from pathlib import Path
import os,subprocess,tempfile
repo=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='astro-boot-') as tmp:
 p=Path(tmp)
 (p/'bootloader.h').write_text('void bootloader_jump(void);\n')
 (p/'hal.h').write_text('''#include <stdint.h>
typedef struct { volatile uint32_t APB1ENR, BDCR; } Rcc;
typedef struct { volatile uint32_t CR; } Pwr;
typedef struct { volatile uint32_t BKP0R, BKP1R; } Rtc;
extern Rcc rcc;
Pwr *get_pwr(void); Rtc *get_rtc(void); void barrier(void); void NVIC_SystemReset(void);
#define RCC (&rcc)
#define PWR get_pwr()
#define RTC get_rtc()
#define RCC_APB1ENR_PWREN (1U<<28)
#define PWR_CR_DBP (1U<<8)
#define RCC_BDCR_RTCSEL (3U<<8)
#define RCC_BDCR_RTCSEL_1 (2U<<8)
#define __DSB() barrier()
''')
 (p/'test.c').write_text('''#include "hal.h"
#include <assert.h>
#include <stdio.h>
#include "bootloader.h"
Rcc rcc; static Pwr pwr,blocked; static Rtc rtc,bad;
static unsigned mode,power_access,rtc_access,barriers,resets;
Pwr *get_pwr(void){ assert(rcc.APB1ENR&RCC_APB1ENR_PWREN); ++power_access; return mode==1&&power_access>1?&blocked:&pwr; }
Rtc *get_rtc(void){ assert(pwr.CR&PWR_CR_DBP); ++rtc_access; return mode==2&&rtc_access>1?&bad:&rtc; }
void barrier(void){ assert(rtc.BKP0R==0x41544b42); ++barriers; }
void NVIC_SystemReset(void){ assert(barriers==1); assert(rtc.BKP0R==0x41544b42); ++resets; }
static void test(unsigned m,unsigned source){mode=m;power_access=rtc_access=barriers=resets=0;rcc.APB1ENR=0x21;rcc.BDCR=0x8023|source;pwr.CR=0x80;blocked.CR=0;rtc.BKP0R=0;rtc.BKP1R=0x12345678;bad.BKP0R=0;
bootloader_jump(); assert(rcc.APB1ENR==((1U<<28)|0x21));assert(rcc.BDCR==(0x8023|source));assert(rtc.BKP1R==0x12345678);assert(pwr.CR==0x180);
if(source==0x100||source==0x300){assert(resets==0&&rtc_access==0&&barriers==0);}else if(m==0){assert(resets==1&&rtc_access==2&&barriers==1);}else if(m==1){assert(resets==0&&rtc_access==0&&barriers==0&&power_access==1025);}else{assert(resets==0&&rtc_access==2&&barriers==1);}}
int main(void){test(0,0);test(0,0x200);test(1,0);test(2,0x200);test(0,0x100);test(0,0x300);test(0,0);puts("PASS ASTRO RTC magic, clock-before-PWR ordering, DBP readiness, readback/barrier-before-reset, bounded failure, preserved backup domain, loader-compatible unselected/LSI sources, other source rejection, repeated requests");}
''')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(p)]
 if os.environ.get('SANITIZE')=='1': flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(['gcc',*flags,str(p/'test.c'),str(repo/'keyboards/alectardy98_ec/me_ec_uf2/astro_bootloader.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
