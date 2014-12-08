

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "common.h"
#include "libco.h"
#include "libretro.h"

#ifndef MAX_PATH
#define MAX_PATH (512)
#endif

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_environment_t environ_cb;

struct retro_perf_callback perf_cb;

static cothread_t main_thread;
static cothread_t cpu_thread;

void switch_to_main_thread(void)
{
   co_switch(main_thread);
}

static inline void switch_to_cpu_thread(void)
{
   co_switch(cpu_thread);
}

static void cpu_thread_entry(void)
{
   execute_arm_translate(execute_cycles);
}

static inline void init_context_switch(void)
{
   main_thread = co_active();
   cpu_thread = co_create(0x20000, cpu_thread_entry);
}

static inline void deinit_context_switch(void)
{
   co_delete(cpu_thread);
}

#ifdef PERF_TEST

extern struct retro_perf_callback perf_cb;

#define RETRO_PERFORMANCE_INIT(X) \
   static struct retro_perf_counter X = {#X}; \
   do { \
      if (!(X).registered) \
         perf_cb.perf_register(&(X)); \
   } while(0)

#define RETRO_PERFORMANCE_START(X) perf_cb.perf_start(&(X))
#define RETRO_PERFORMANCE_STOP(X) perf_cb.perf_stop(&(X))
#else
#define RETRO_PERFORMANCE_INIT(X)
#define RETRO_PERFORMANCE_START(X)
#define RETRO_PERFORMANCE_STOP(X)

#endif

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "TempGBA";
   info->library_version = "v0.0.1";
   info->need_fullpath = true;
   info->block_extract = false;
   info->valid_extensions = "gba|bin|agb|gbz" ;
}


void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width = GBA_SCREEN_WIDTH;
   info->geometry.base_height = GBA_SCREEN_HEIGHT;
   info->geometry.max_width = GBA_SCREEN_WIDTH;
   info->geometry.max_height = GBA_SCREEN_HEIGHT;
   info->geometry.aspect_ratio = 0;
   info->timing.fps = ((float) (16* 1024 * 1024)) / (308 * 228 * 4); // 59.72750057 hz
   info->timing.sample_rate = 44100;
//   info->timing.sample_rate = 32 * 1024;
}


void retro_init()
{
   init_gamepak_buffer();
   init_sound(1);
}

void retro_deinit()
{
   perf_cb.perf_log();
   memory_term();
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback log;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);

}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

void retro_set_controller_port_device(unsigned port, unsigned device) {}

void retro_reset()
{
   deinit_context_switch();

   update_backup();
   reset_gba();

   init_context_switch();
}


size_t retro_serialize_size()
{
//   return SAVESTATE_SIZE;
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
//   if (size < SAVESTATE_SIZE)
      return false;

//   gba_save_state(data);

//   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
//   if (size < SAVESTATE_SIZE)
      return false;

//   gba_load_state(data);

//   return true;
}

void retro_cheat_reset() {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

void error_msg(const char *text)
{
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, text);
}

void info_msg(const char *text)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, text);
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');

   if (base)
      *base = '\0';
   else
      strncpy(buf, ".", size);
}

bool retro_load_game(const struct retro_game_info *info)
{
   char filename_bios[MAX_PATH];
   const char *dir = NULL;

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[TempGBA]: 0RGB1555 is not supported.\n");
      return false;
   }

   extract_directory(main_path,info->path,sizeof(main_path));

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      strncpy(filename_bios, dir, sizeof(filename_bios));
   else
      strncpy(filename_bios, main_path, sizeof(filename_bios));

   strncat(filename_bios, "/gba_bios.bin",sizeof(filename_bios));


//   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
//      strncpy(dir_save, dir, sizeof(dir_save));
//   else
//      strncpy(dir_save, main_path, sizeof(dir_save));

//   strncat(dir_save, "/",sizeof(dir_save));

   strncat(main_path, "/",sizeof(main_path));

   if (load_bios(filename_bios) < 0)
   {
     error_msg("Could not load BIOS image file.\n");
     return false;
   }

   gamepak_filename[0] = 0;

   if (load_gamepak(info->path) < 0)
   {
     error_msg("Could not load the game file.\n");
     return false;
   }

   reset_gba();

   init_context_switch();

   return true;
}


bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{ return false; }

void retro_unload_game()
{
   deinit_context_switch();
   update_backup();
}

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id)
{
//   switch (id)
//   {
//   case RETRO_MEMORY_SAVE_RAM:
//      return gamepak_backup;
//   }

   return 0;
}

size_t retro_get_memory_size(unsigned id)
{
//   switch (id)
//   {
//   case RETRO_MEMORY_SAVE_RAM:
//      switch(backup_type)
//      {
//      case BACKUP_SRAM:
//         return sram_size;

//      case BACKUP_FLASH:
//         return flash_size;

//      case BACKUP_EEPROM:
//         return eeprom_size;

//      case BACKUP_NONE:
//         return 0x0;

//      default:
//         return 0x8000;
//      }
//   }

   return 0;
}

static void check_variables(void)
{

}

void retro_run()
{
   bool updated = false;

   input_poll_cb();

   switch_to_cpu_thread();

   update_input();

   render_audio();

   video_cb(gba_screen_pixels, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, GBA_SCREEN_PITCH * 2);

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
       check_variables();

}

unsigned retro_api_version() { return RETRO_API_VERSION; }
