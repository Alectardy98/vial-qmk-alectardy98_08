#include <stdint.h>
#include <string.h>
#include "doomdef.h"
#include "d_event.h"
#include "v_video.h"
#include "r_main.h"
extern int detailLevel;
#include "w_wad.h"
extern void my2k_stream_read(unsigned,void*,unsigned);
#include "aux_work.h"
extern void D_PostEvent(event_t *);
extern boolean singletics;
static uint16_t target_palette[256];
static uint8_t current_buttons,previous_buttons;
static int detail_combo;
unsigned my2k_doom_detail(void){return detailLevel!=0;}
extern int my2k_doom_present(const byte *,const uint16_t *);
void my2k_doom_set_buttons(unsigned b){current_buttons=(uint8_t)b;}
void I_SetPalette(byte *p){for(unsigned i=0;i<256;i++)target_palette[i]=(uint16_t)((p[3*i]>>3)<<11|(p[3*i+1]>>2)<<5|(p[3*i+2]>>3));}
void I_UpdateNoBlit(void){}
void I_InitGraphics(void){byte palette[768];int n=W_GetNumForName("PLAYPAL");my2k_stream_read(lumpinfo[n].position,palette,sizeof palette);I_SetPalette(palette);detailLevel=1;R_SetViewSize(11,1);singletics=true;}
void I_StartTic(void){
 unsigned input=current_buttons;int combo=(input&12u)==12u;
 if(combo&&!detail_combo){detailLevel^=1;R_SetViewSize(11,detailLevel);}detail_combo=combo;
 if(combo)input&=~12u;
 int keys[8]={KEY_RCTRL,' ',KEY_RSHIFT,KEY_ENTER,KEY_UPARROW,KEY_DOWNARROW,KEY_LEFTARROW,KEY_RIGHTARROW};
 for(unsigned i=0;i<8;i++)if((input^previous_buttons)&(1u<<i)){event_t e={.type=(input&(1u<<i))?ev_keydown:ev_keyup,.data1=keys[i]};D_PostEvent(&e);}
 previous_buttons=(uint8_t)input;aux_work_checkpoint();
}
void I_ReadScreen(byte *p){memcpy(p,screens[0],SCREENWIDTH*SCREENHEIGHT);}
void I_StartFrame(void){}
void I_ShutdownGraphics(void){}
void I_FinishUpdate(void){if(!my2k_doom_present(screens[0],target_palette)){extern void I_Error(char*,...);I_Error("display transfer failed");}}
