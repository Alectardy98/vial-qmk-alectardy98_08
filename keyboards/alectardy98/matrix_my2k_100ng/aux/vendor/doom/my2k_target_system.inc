/* MY2K AUX platform: bounded engine heap, supervisor-owned lifecycle. */
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdio.h>
#include "doomdef.h"
#include "d_ticcmd.h"
#include "aux_platform.h"
#include "aux_work.h"
byte CombinedScreens[SCREENWIDTH*SCREENHEIGHT];
byte DOOMHeap[FIXED_HEAP];
ticcmd_t emptycmd;
jmp_buf my2k_doom_error_jump;
char my2k_doom_error_text[96];
int mb_used=0;
void I_Tactile(int on,int off,int total){(void)on;(void)off;(void)total;}
ticcmd_t *I_BaseTiccmd(void){return &emptycmd;}
int I_GetHeapSize(void){return FIXED_HEAP;}
byte *I_ZoneBase(int *size){*size=FIXED_HEAP;return DOOMHeap;}
int I_GetTime(void){return (int)((uint64_t)aux_millis()*35u/1000u);}
void I_Init(void){}
void I_Quit(void){longjmp(my2k_doom_error_jump,2);}
void I_WaitVBL(int count){(void)count;aux_work_checkpoint();}
void I_BeginRead(void){}
void I_EndRead(void){}
byte *I_AllocLow(int n){if(n<0||n>(int)sizeof CombinedScreens)longjmp(my2k_doom_error_jump,3);memset(CombinedScreens,0,n);return CombinedScreens;}
void I_Error(char *fmt,...){va_list args;va_start(args,fmt);vsnprintf(my2k_doom_error_text,sizeof my2k_doom_error_text,fmt,args);va_end(args);longjmp(my2k_doom_error_jump,1);}
