/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"
#ifndef __LIBRETRO__
#include "frontend/main.h"
#include "frontend/libpicofe/input.h"
#endif

static u32 old_key = 0;
static retro_input_state_t input_state_cb;

void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static void trigger_key(u32 key)
{
  u32 p1_cnt = io_registers[REG_P1CNT];

  if((p1_cnt >> 14) & 0x01)
  {
    u32 key_intersection = (p1_cnt & key) & 0x3FF;

    if(p1_cnt >> 15)
    {
      if(key_intersection == (p1_cnt & 0x3FF))
        raise_interrupt(IRQ_KEYPAD);
    }
    else
    {
      if(key_intersection)
        raise_interrupt(IRQ_KEYPAD);
    }
  }
}

#ifdef __LIBRETRO__
u32 update_input(void)
{
   unsigned i;
   uint32_t new_key = 0;

   if (!input_state_cb)
      return 0;

   for (i = 0; i < sizeof(btn_map) / sizeof(map); i++)
      new_key |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, btn_map[i].retropad) ? btn_map[i].gba : 0;

   if ((new_key | old_key) != old_key)
      trigger_key(new_key);

   old_key = new_key;
   io_registers[REG_P1] = (~old_key) & 0x3FF;

   return 0;
}
#else

u32 update_input(void)
{
  int actions[IN_BINDTYPE_COUNT] = { 0, };
  uint32_t new_key = 0;
  unsigned int emu_act;
  int which = EACTION_NONE;

  in_update(actions);
  emu_act = actions[IN_BINDTYPE_EMU];
  if (emu_act) {
    for (; !(emu_act & 1); emu_act >>= 1, which++)
			;
		emu_act = which;
  }
  handle_emu_action(which);

  new_key = actions[IN_BINDTYPE_PLAYER12];

  if ((new_key | old_key) != old_key)
    trigger_key(new_key);

  old_key = new_key;
  io_registers[REG_P1] = (~old_key) & 0x3FF;

  return 0;
}
#endif

#define input_savestate_builder(type)   \
void input_##type##_savestate(void)     \
{                                       \
  state_mem_##type##_variable(old_key); \
}

input_savestate_builder(read)
input_savestate_builder(write)
