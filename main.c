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
#include <ctype.h>

#ifdef PSP_BUILD
void vblank_interrupt_handler(u32 sub, u32 *parg);
#endif

timer_type timer[4];

//debug_state current_debug_state = COUNTDOWN_BREAKPOINT;
//debug_state current_debug_state = PC_BREAKPOINT;
u32 breakpoint_value = 0x7c5000;
debug_state current_debug_state = RUN;
//debug_state current_debug_state = STEP_RUN;

//u32 breakpoint_value = 0;

frameskip_type current_frameskip_type = auto_frameskip;
u32 global_cycles_per_instruction = 1;

u32 random_skip = 0;
u32 fps_debug = 0;

u32 frameskip_value = 2;

u64 last_frame_interval_timestamp;

u32 skip_next_frame = 0;

u32 frameskip_counter = 0;

u32 cpu_ticks = 0;
u32 frame_ticks = 0;

u32 execute_cycles = 960;
s32 video_count = 960;
u32 ticks;

u32 arm_frame = 0;
u32 thumb_frame = 0;
u32 last_frame = 0;

u32 cycle_memory_access = 0;
u32 cycle_pc_relative_access = 0;
u32 cycle_sp_relative_access = 0;
u32 cycle_block_memory_access = 0;
u32 cycle_block_memory_sp_access = 0;
u32 cycle_block_memory_words = 0;
u32 cycle_dma16_words = 0;
u32 cycle_dma32_words = 0;
u32 flush_ram_count = 0;
u32 gbc_update_count = 0;
u32 oam_update_count = 0;

u32 synchronize_flag = 1;

u32 update_backup_flag = 1;
char main_path[512];

void trigger_ext_event();

#define check_count(count_var)                                                \
  if(count_var < execute_cycles)                                              \
    execute_cycles = count_var;                                               \

#define check_timer(timer_number)                                             \
  if(timer[timer_number].status == TIMER_PRESCALE)                            \
    check_count(timer[timer_number].count);                                   \

#define update_timer(timer_number)                                            \
  if(timer[timer_number].status != TIMER_INACTIVE)                            \
  {                                                                           \
    if(timer[timer_number].status != TIMER_CASCADE)                           \
    {                                                                         \
      timer[timer_number].count -= execute_cycles;                            \
      io_registers[REG_TM##timer_number##D] =                                 \
       -(timer[timer_number].count >> timer[timer_number].prescale);          \
    }                                                                         \
                                                                              \
    if(timer[timer_number].count <= 0)                                        \
    {                                                                         \
      if(timer[timer_number].irq == TIMER_TRIGGER_IRQ)                        \
        irq_raised |= IRQ_TIMER##timer_number;                                \
                                                                              \
      if((timer_number != 3) &&                                               \
       (timer[timer_number + 1].status == TIMER_CASCADE))                     \
      {                                                                       \
        timer[timer_number + 1].count--;                                      \
        io_registers[REG_TM0D + (timer_number + 1) * 2] =                     \
         -(timer[timer_number + 1].count);                                    \
      }                                                                       \
                                                                              \
      if(timer_number < 2)                                                    \
      {                                                                       \
        if(timer[timer_number].direct_sound_channels & 0x01)                  \
          sound_timer(timer[timer_number].frequency_step, 0);                 \
                                                                              \
        if(timer[timer_number].direct_sound_channels & 0x02)                  \
          sound_timer(timer[timer_number].frequency_step, 1);                 \
      }                                                                       \
                                                                              \
      timer[timer_number].count +=                                            \
       (timer[timer_number].reload << timer[timer_number].prescale);          \
    }                                                                         \
  }                                                                           \

void init_main()
{
  u32 i;

  skip_next_frame = 0;

  for(i = 0; i < 4; i++)
  {
    dma[i].start_type = DMA_INACTIVE;
    dma[i].direct_sound_channel = DMA_NO_DIRECT_SOUND;
    timer[i].status = TIMER_INACTIVE;
    timer[i].reload = 0x10000;
    timer[i].stop_cpu_ticks = 0;
  }

  timer[0].direct_sound_channels = TIMER_DS_CHANNEL_BOTH;
  timer[1].direct_sound_channels = TIMER_DS_CHANNEL_NONE;

  cpu_ticks = 0;
  frame_ticks = 0;

  execute_cycles = 960;
  video_count = 960;

  flush_translation_cache_rom();
  flush_translation_cache_ram();
  flush_translation_cache_bios();
}

void print_memory_stats(u32 *counter, u32 *region_stats, char *stats_str)
{
  u32 other_region_counter = region_stats[0x1] + region_stats[0xE] +
   region_stats[0xF];
  u32 rom_region_counter = region_stats[0x8] + region_stats[0x9] +
   region_stats[0xA] + region_stats[0xB] + region_stats[0xC] +
   region_stats[0xD];
  u32 _counter = *counter;

  printf("memory access stats: %s (out of %d)\n", stats_str, _counter);
  printf("bios: %f%%\tiwram: %f%%\tewram: %f%%\tvram: %f\n",
   region_stats[0x0] * 100.0 / _counter, region_stats[0x3] * 100.0 /
   _counter,
   region_stats[0x2] * 100.0 / _counter, region_stats[0x6] * 100.0 /
   _counter);

  printf("oam: %f%%\tpalette: %f%%\trom: %f%%\tother: %f%%\n",
   region_stats[0x7] * 100.0 / _counter, region_stats[0x5] * 100.0 /
   _counter,
   rom_region_counter * 100.0 / _counter, other_region_counter * 100.0 /
   _counter);

  *counter = 0;
  memset(region_stats, 0, sizeof(u32) * 16);
}

u32 event_cycles = 0;
const u32 event_cycles_trigger = 60 * 5;
u32 no_alpha = 0;

void trigger_ext_event()
{
  static u32 event_number = 0;
  static u64 benchmark_ticks[16];
  u64 new_ticks;

  return;

  if(event_number)
  {
    get_ticks_us(&new_ticks);
    benchmark_ticks[event_number - 1] =
     new_ticks - benchmark_ticks[event_number - 1];
  }

  current_frameskip_type = no_frameskip;
  no_alpha = 0;
  synchronize_flag = 0;

  switch(event_number)
  {
    case 0:
      // Full benchmark, run normally
      break;

    case 1:
      // No alpha blending
      no_alpha = 1;
      break;

    case 2:
      // No video benchmark
      // Set frameskip really high + manual
      current_frameskip_type = manual_frameskip;
      frameskip_value = 1000000;
      break;

    case 3:
      // No CPU benchmark
      // Put CPU in halt mode, put it in IRQ mode with interrupts off
      reg[CPU_HALT_STATE] = CPU_HALT;
      reg[REG_CPSR] = 0xD2;
      break;

    case 4:
      // No CPU or video benchmark
      reg[CPU_HALT_STATE] = CPU_HALT;
      reg[REG_CPSR] = 0xD2;
      current_frameskip_type = manual_frameskip;
      frameskip_value = 1000000;
      break;

    case 5:
    {
      // Done
      char *print_strings[] =
      {
        "Full test   ",
        "No blending ",
        "No video    ",
        "No CPU      ",
        "No CPU/video",
        "CPU speed   ",
        "Video speed ",
        "Alpha cost  "
      };
      u32 i;

      benchmark_ticks[6] = benchmark_ticks[0] - benchmark_ticks[2];
      benchmark_ticks[5] = benchmark_ticks[0] - benchmark_ticks[4] -
       benchmark_ticks[6];
      benchmark_ticks[7] = benchmark_ticks[0] - benchmark_ticks[1];

      printf("Benchmark results (%d frames): \n", event_cycles_trigger);
      for(i = 0; i < 8; i++)
      {
        printf("   %s: %d ms (%f ms per frame)\n",
         print_strings[i], (u32)benchmark_ticks[i] / 1000,
         (float)(benchmark_ticks[i] / (1000.0 * event_cycles_trigger)));
        if(i == 4)
          printf("\n");
      }
      quit();
    }
  }

  event_cycles = 0;

  get_ticks_us(benchmark_ticks + event_number);
  event_number++;
}

u32 update_gba()
{
  irq_type irq_raised = IRQ_NONE;

  do
  {
    cpu_ticks += execute_cycles;

    reg[CHANGED_PC_STATUS] = 0;

    if(gbc_sound_update)
    {
      gbc_update_count++;
      update_gbc_sound(cpu_ticks);
      gbc_sound_update = 0;
    }

    update_timer(0);
    update_timer(1);
    update_timer(2);
    update_timer(3);

    video_count -= execute_cycles;

    if(video_count <= 0)
    {
      u32 vcount = io_registers[REG_VCOUNT];
      u32 dispstat = io_registers[REG_DISPSTAT];

      if((dispstat & 0x02) == 0)
      {
        // Transition from hrefresh to hblank
        video_count += (272);
        dispstat |= 0x02;

        if((dispstat & 0x01) == 0)
        {
          u32 i;
          if(oam_update)
            oam_update_count++;

          if(no_alpha)
            io_registers[REG_BLDCNT] = 0;
          update_scanline();

          // If in visible area also fire HDMA
          for(i = 0; i < 4; i++)
          {
            if(dma[i].start_type == DMA_START_HBLANK)
              dma_transfer(dma + i);
          }
        }

        if(dispstat & 0x10)
          irq_raised |= IRQ_HBLANK;
      }
      else
      {
        // Transition from hblank to next line
        video_count += 960;
        dispstat &= ~0x02;

        vcount++;

        if(vcount == 160)
        {
          // Transition from vrefresh to vblank
          u32 i;

          dispstat |= 0x01;
          if(dispstat & 0x8)
          {
            irq_raised |= IRQ_VBLANK;
          }

          affine_reference_x[0] =
           (s32)(address32(io_registers, 0x28) << 4) >> 4;
          affine_reference_y[0] =
           (s32)(address32(io_registers, 0x2C) << 4) >> 4;
          affine_reference_x[1] =
           (s32)(address32(io_registers, 0x38) << 4) >> 4;
          affine_reference_y[1] =
           (s32)(address32(io_registers, 0x3C) << 4) >> 4;

          for(i = 0; i < 4; i++)
          {
            if(dma[i].start_type == DMA_START_VBLANK)
              dma_transfer(dma + i);
          }
        }
        else

        if(vcount == 228)
        {
          // Transition from vblank to next screen
          dispstat &= ~0x01;
          frame_ticks++;

/*        printf("frame update (%x), %d instructions total, %d RAM flushes\n",
           reg[REG_PC], instruction_count - last_frame, flush_ram_count);
          last_frame = instruction_count;
*/
/*          printf("%d gbc audio updates\n", gbc_update_count);
          printf("%d oam updates\n", oam_update_count); */
          gbc_update_count = 0;
          oam_update_count = 0;
          flush_ram_count = 0;

#ifdef __LIBRETRO__
          switch_to_main_thread();

          update_gbc_sound(cpu_ticks);
          gbc_sound_update = 0;
#else
          if(update_input())
            continue;

          update_gbc_sound(cpu_ticks);

          update_screen();

          synchronize();

          if(update_backup_flag)
            update_backup();
#endif
          process_cheats();

          event_cycles++;
          if(event_cycles == event_cycles_trigger)
          {
            trigger_ext_event();
            continue;
          }

          vcount = 0;
        }

        if(vcount == (dispstat >> 8))
        {
          // vcount trigger
          dispstat |= 0x04;
          if(dispstat & 0x20)
          {
            irq_raised |= IRQ_VCOUNT;
          }
        }
        else
        {
          dispstat &= ~0x04;
        }

        io_registers[REG_VCOUNT] = vcount;
      }
      io_registers[REG_DISPSTAT] = dispstat;
    }

    if(irq_raised)
      raise_interrupt(irq_raised);

    execute_cycles = video_count;

    check_timer(0);
    check_timer(1);
    check_timer(2);
    check_timer(3);
  } while(reg[CPU_HALT_STATE] != CPU_ACTIVE);

  return execute_cycles;
}

#ifdef PSP_BUILD

u32 real_frame_count = 0;
u32 virtual_frame_count = 0;
u32 num_skipped_frames = 0;

void vblank_interrupt_handler(u32 sub, u32 *parg)
{
  real_frame_count++;
}

void synchronize()
{
  char char_buffer[64];
  u64 new_ticks, time_delta;
  s32 used_frameskip = frameskip_value;

  if(!synchronize_flag)
  {
    used_frameskip = 4;
    virtual_frame_count = real_frame_count - 1;
  }

  skip_next_frame = 0;

  virtual_frame_count++;

  if(real_frame_count >= virtual_frame_count)
  {
    if((real_frame_count > virtual_frame_count) &&
     (current_frameskip_type == auto_frameskip) &&
     (num_skipped_frames < frameskip_value))
    {
      skip_next_frame = 1;
      num_skipped_frames++;
    }
    else
    {
      virtual_frame_count = real_frame_count;
      num_skipped_frames = 0;
    }

    // Here so that the home button return will eventually work.
    // If it's not running fullspeed anyway this won't really hurt
    // it much more.

    delay_us(1);
  }
  else
  {
    if(synchronize_flag)
      sceDisplayWaitVblankStart();
  }

  if(current_frameskip_type == manual_frameskip)
  {
    frameskip_counter = (frameskip_counter + 1) %
     (used_frameskip + 1);
    if(random_skip)
    {
      if(frameskip_counter != (rand() % (used_frameskip + 1)))
        skip_next_frame = 1;
    }
    else
    {
      if(frameskip_counter)
        skip_next_frame = 1;
    }
  }

/*  sprintf(char_buffer, "%08d %08d %d %d %d\n",
   real_frame_count, virtual_frame_count, num_skipped_frames,
   real_frame_count - virtual_frame_count, skip_next_frame);
  print_string(char_buffer, 0xFFFF, 0x0000, 0, 10); */

/*
    sprintf(char_buffer, "%02d %02d %06d %07d", frameskip, (u32)ms_needed,
     ram_translation_ptr - ram_translation_cache, rom_translation_ptr -
     rom_translation_cache);
    print_string(char_buffer, 0xFFFF, 0x0000, 0, 0);
*/
}
#endif

void quit()
{
  if(!update_backup_flag)
    update_backup_force();

  sound_exit();
}

void reset_gba()
{
  init_main();
  init_memory();
  init_cpu();
  reset_sound();
}

#ifdef PSP_BUILD

u32 file_length(char *filename, s32 dummy)
{
  SceIoStat stats;
  sceIoGetstat(filename, &stats);
  return stats.st_size;
}

void delay_us(u32 us_count)
{
  sceKernelDelayThread(us_count);
}

void get_ticks_us(u64 *tick_return)
{
  u64 ticks;
  sceRtcGetCurrentTick(&ticks);

  *tick_return = (ticks * 1000000) / sceRtcGetTickResolution();
}

#else

u32 file_length(const char *dummy, FILE *fp)
{
  u32 length;

  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  return length;
}

#endif

void change_ext(const char *src, char *buffer, const char *extension)
{
  char *dot_position;
  strcpy(buffer, src);
  dot_position = strrchr(buffer, '.');

  if(dot_position)
    strcpy(dot_position, extension);
}

// make path: <main_path>/<romname>.<ext>
void make_rpath(char *buff, size_t size, const char *ext)
{
  char *p;
  p = strrchr(gamepak_filename, PATH_SEPARATOR_CHAR);
  if (p == NULL)
    p = gamepak_filename;

  snprintf(buff, size, "%s/%s", main_path, p);
  p = strrchr(buff, '.');
  if (p != NULL)
    strcpy(p, ext);
}

#define main_savestate_builder(type)                                          \
void main_##type##_savestate(file_tag_type savestate_file)                    \
{                                                                             \
  file_##type##_variable(savestate_file, cpu_ticks);                          \
  file_##type##_variable(savestate_file, execute_cycles);                     \
  file_##type##_variable(savestate_file, video_count);                        \
  file_##type##_array(savestate_file, timer);                                 \
}                                                                             \

main_savestate_builder(read);
main_savestate_builder(write_mem);


void printout(void *str, u32 val)
{
  printf(str, val);
}
