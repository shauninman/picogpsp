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

// Special thanks to psp298 for the analog->dpad code!

void trigger_key(u32 key)
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

u32 key = 0;

u32 global_enable_analog = 1;
u32 analog_sensitivity_level = 4;

typedef enum
{
  BUTTON_NOT_HELD,
  BUTTON_HELD_INITIAL,
  BUTTON_HELD_REPEAT
} button_repeat_state_type;


// These define autorepeat values (in microseconds), tweak as necessary.

#define BUTTON_REPEAT_START    200000
#define BUTTON_REPEAT_CONTINUE 50000

button_repeat_state_type button_repeat_state = BUTTON_NOT_HELD;
u32 button_repeat = 0;
gui_action_type cursor_repeat = CURSOR_NONE;

static retro_input_state_t input_state_cb;
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

u32 update_input(void)
{
//   return;
   unsigned i;
   uint32_t new_key = 0;

   if (!input_state_cb)
      return 0;

   for (i = 0; i < sizeof(btn_map) / sizeof(map); i++)
      new_key |= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, btn_map[i].retropad) ? btn_map[i].gba : 0;

   if ((new_key | key) != key)
      trigger_key(new_key);

   key = new_key;
   io_registers[REG_P1] = (~key) & 0x3FF;

   return 0;
}

#define input_savestate_builder(type)                                         \
void input_##type##_savestate(file_tag_type savestate_file)                   \
{                                                                             \
  file_##type##_variable(savestate_file, key);                                \
}                                                                             \

input_savestate_builder(read);
input_savestate_builder(write_mem);

