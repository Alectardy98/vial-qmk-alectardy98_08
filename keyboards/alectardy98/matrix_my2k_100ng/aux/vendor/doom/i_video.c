#ifdef MY2K_DOOM_TARGET
#include "my2k_target_video.inc"
#else
/* MY2K headless engineering adapter; engine retains its original license. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "doomdef.h"
#include "v_video.h"
#include "z_zone.h"
#include "d_event.h"
extern void D_PostEvent(event_t*);
extern boolean singletics;
static unsigned frames;
void I_SetPalette(byte *p){(void)p;}
void I_UpdateNoBlit(void){}
void I_InitGraphics(void){if(getenv("MY2K_FIXED_TICS"))singletics=true;if(getenv("MY2K_LOW_DETAIL")){extern void R_SetViewSize(int,int);R_SetViewSize(11,1);}}
void I_StartTic(void){
 if(!getenv("MY2K_SCRIPT_INPUT"))return;
 static unsigned tic;
 unsigned phase=(tic++/35)%12;
 static unsigned old;
 unsigned mask=(phase<4?1u:0)|(phase>=4&&phase<8?2u:0)|(phase>=8?4u:0);
 int keys[3]={KEY_UPARROW,KEY_RIGHTARROW,KEY_RCTRL};
 for(unsigned i=0;i<3;i++)if((old^mask)&(1u<<i)){event_t e={.type=(mask&(1u<<i))?ev_keydown:ev_keyup,.data1=keys[i]};D_PostEvent(&e);}old=mask;
}
void I_ReadScreen(byte *p){memcpy(p,screens[0],SCREENWIDTH*SCREENHEIGHT);}
void I_StartFrame(void){}
void I_ShutdownGraphics(void){}
void I_FinishUpdate(void){if(++frames==(getenv("MY2K_TEST_FRAMES")?strtoul(getenv("MY2K_TEST_FRAMES"),0,10):60)){FILE *f=fopen("../doom128-frame.pgm","wb");if(f){fprintf(f,"P5\n%d %d\n255\n",SCREENWIDTH,SCREENHEIGHT);fwrite(screens[0],1,SCREENWIDTH*SCREENHEIGHT,f);fclose(f);}
 #ifdef MY2K_DOOM_STREAM
 extern void my2k_stream_report(void);my2k_stream_report();
#endif
 Z_DumpHeap(0,100);printf("MY2K_HEADLESS_%u_COMPLETED_FRAMES\n",frames);exit(0);}}

#endif
