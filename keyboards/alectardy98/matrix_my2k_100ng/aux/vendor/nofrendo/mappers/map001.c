/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** map1.c
**
** mapper 1 interface
** $Id: map001.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <string.h>
#include <noftypes.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes6502.h>

/* MY2K: reapply every mapping after a serial register commit/reset. The
 * upstream implementation left stale mappings after control-only writes. */
static uint8 regs[4],latch,bitcount;
static uint32 last_cycle;
static bool cycle_valid;
static void map1_apply(void)
{
 unsigned control=regs[0],mirror=control&3u;
 if(mirror<2)ppu_mirror(mirror,mirror,mirror,mirror);
 else if(mirror==2)ppu_mirror(0,1,0,1);else ppu_mirror(0,0,1,1);
 if(control&16u){mmc_bankvrom(4,0,regs[1]);mmc_bankvrom(4,0x1000,regs[2]);}
 else mmc_bankvrom(8,0,regs[1]>>1);
 unsigned total=(unsigned)mmc_getinfo()->rom_banks;
 unsigned outer=total>16u?(regs[1]&16u):0u;
 unsigned bank=outer|(regs[3]&15u),last=outer|15u;
 switch((control>>2)&3u){
 case 0:case 1:mmc_bankrom(32,0x8000,bank>>1);break;
 case 2:mmc_bankrom(16,0x8000,outer);mmc_bankrom(16,0xc000,bank);break;
 default:mmc_bankrom(16,0x8000,bank);mmc_bankrom(16,0xc000,last);break;
 }
}
static void map1_write(uint32 address,uint8 value)
{
 uint32 cycle=nes6502_getcycles(false);
 bool consecutive=cycle_valid&&cycle-last_cycle==1u;
 last_cycle=cycle;cycle_valid=true;
 if(value&128u){bitcount=latch=0;regs[0]|=12u;map1_apply();return;}
 if(consecutive)return;
 latch|=(value&1u)<<bitcount++;
 if(bitcount==5){regs[(address>>13)&3u]=latch;bitcount=latch=0;map1_apply();}
}
static void map1_init(void)
{
 memset(regs,0,sizeof regs);regs[0]=12;bitcount=latch=0;cycle_valid=false;map1_apply();
}

static void map1_getstate(SnssMapperBlock *state)
{
   state->extraData.mapper1.registers[0] = regs[0];
   state->extraData.mapper1.registers[1] = regs[1];
   state->extraData.mapper1.registers[2] = regs[2];
   state->extraData.mapper1.registers[3] = regs[3];
   state->extraData.mapper1.latch = latch;
   state->extraData.mapper1.numberOfBits = bitcount;
}


static void map1_setstate(SnssMapperBlock *state)
{
   regs[0] = state->extraData.mapper1.registers[0];
   regs[1] = state->extraData.mapper1.registers[1];
   regs[2] = state->extraData.mapper1.registers[2];
   regs[3] = state->extraData.mapper1.registers[3];
   latch = state->extraData.mapper1.latch;
   bitcount = state->extraData.mapper1.numberOfBits;
   map1_apply();
}

static map_memwrite map1_memwrite[] =
{
   { 0x8000, 0xFFFF, map1_write },
   {     -1,     -1, NULL }
};

mapintf_t map1_intf =
{
   1, /* mapper number */
   "MMC1", /* mapper name */
   map1_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   map1_getstate, /* get state (snss) */
   map1_setstate, /* set state (snss) */
   NULL, /* memory read structure */
   map1_memwrite, /* memory write structure */
   NULL /* external sound device */
};

/*
** $Log: map001.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.1  2000/10/24 12:19:32  matt
** changed directory structure
**
** Revision 1.8  2000/10/22 19:46:50  matt
** mirroring bugfix
**
** Revision 1.7  2000/10/22 19:17:46  matt
** mapper cleanups galore
**
** Revision 1.6  2000/10/22 15:03:13  matt
** simplified mirroring
**
** Revision 1.5  2000/10/21 19:33:38  matt
** many more cleanups
**
** Revision 1.4  2000/07/15 23:52:19  matt
** rounded out a bunch more mapper interfaces
**
** Revision 1.3  2000/07/10 05:29:03  matt
** cleaned up some mirroring issues
**
** Revision 1.2  2000/07/06 02:48:43  matt
** clearly labelled structure members
**
** Revision 1.1  2000/07/04 23:11:45  matt
** initial revision
**
*/
