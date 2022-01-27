#include <stdio.h>
#include <string.h>
#include "common.h"
#include "main.h"
#include "memmap.h"
#include "frontend/menu.h"
#include "frontend/plat.h"
#include "frontend/libpicofe/plat.h"

#include <dlfcn.h>
#include <mmenu.h>
#include <SDL/SDL.h>
static void* mmenu = NULL;
static int resume_slot = -1;
char rom_path[MAXPATHLEN];
char save_template_path[MAXPATHLEN];

/* Percentage of free space allowed in the audio buffer before
 * skipping frames. Lower numbers mean more skipping but smoother
 * audio, since the buffer will stay closer to filled. */
#define FRAMESKIP_UNDERRUN_THRESHOLD 0.5

int should_quit = 0;

u32 idle_loop_target_pc = 0xFFFFFFFF;
u32 iwram_stack_optimize = 1;
u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];
u32 translation_gate_targets = 0;

uint16_t *gba_screen_pixels_prev = NULL;
uint16_t *gba_processed_pixels = NULL;

int use_libretro_save_method = 0;
bios_type selected_bios = auto_detect;
boot_mode selected_boot_mode = boot_game;

u32 skip_next_frame = 0;

int dynarec_enable;
int state_slot;
frameskip_style_t frameskip_style;
scaling_mode_t scaling_mode;
int max_frameskip;
int color_correct;
int lcd_blend;
int show_fps;
int limit_frames;

static float vsyncsps = 0.0;
static float rendersps = 0.0;

void quit();

void gamepak_related_name(char *buf, size_t len, char *new_extension)
{
  char root_dir[512];
  char filename[512];
  char *p;

  plat_get_root_dir(root_dir, len);
  p = strrchr(gamepak_filename, PATH_SEPARATOR_CHAR);

  if (p)
    p++;
  else
    p = gamepak_filename;
  strncpy(filename, p, sizeof(filename));
  filename[sizeof(filename) - 1] = 0;
  p = strrchr(filename, '.');
  if (p)
    *p = 0;

  snprintf(buf, len, "%s%s%s", root_dir, filename, new_extension);
}

void toggle_fast_forward(int force_off)
{
  static frameskip_style_t frameskip_style_was;
  static int max_frameskip_was;
  static int limit_frames_was;
  static int global_process_audio_was;
  static int fast_forward;

  if (force_off && !fast_forward)
    return;

  fast_forward = !fast_forward;

  if (fast_forward) {
    frameskip_style_was = frameskip_style;
    max_frameskip_was = max_frameskip;
    limit_frames_was = limit_frames;
    global_process_audio_was = global_process_audio;

    frameskip_style = FRAMESKIP_MANUAL;
    max_frameskip = 5;
    limit_frames = 0;
    global_process_audio = 0;
  } else {
    frameskip_style = frameskip_style_was;
    max_frameskip = max_frameskip_was;
    limit_frames = limit_frames_was;
    global_process_audio = global_process_audio_was;
  }
}

void state_file_name(char *buf, size_t len, unsigned state_slot)
{
  char ext[20];
  snprintf(ext, sizeof(ext), ".st%d", state_slot);

  gamepak_related_name(buf, len, ext);
}

void config_file_name(char *buf, size_t len, int is_game)
{
  char root_dir[MAXPATHLEN];

  if (is_game) {
    gamepak_related_name(buf, len, ".cfg");
  } else {
    plat_get_root_dir(root_dir, MAXPATHLEN);
    snprintf(buf, len, "%s%s", root_dir, "picogpsp.cfg");
  }
}

void handle_emu_action(emu_action action)
{
  static frameskip_style_t prev_frameskip_style;
  static emu_action prev_action = EACTION_NONE;
  if (prev_action != EACTION_NONE && prev_action == action) return;

  switch (action)
  {
    case EACTION_NONE:
      break;
    case EACTION_QUIT:
      should_quit = 1;
      break;
    case EACTION_TOGGLE_FPS:
      show_fps = !show_fps;
      /* Force the hud to clear */
      plat_video_set_msg(" ");
      break;
    case EACTION_SAVE_STATE:
      save_state_file(0);
      break;
    case EACTION_LOAD_STATE:
      load_state_file(0);
      break;
    case EACTION_TOGGLE_FSKIP:
      if (prev_frameskip_style == FRAMESKIP_NONE)
        prev_frameskip_style = FRAMESKIP_AUTO;

      if (frameskip_style == FRAMESKIP_NONE) {
        frameskip_style = prev_frameskip_style;
      } else {
        prev_frameskip_style = frameskip_style;
        frameskip_style = FRAMESKIP_NONE;
      }
      break;
    case EACTION_TOGGLE_FF:
      toggle_fast_forward(0);
      break;
    case EACTION_MENU:
      toggle_fast_forward(1);
      update_backup();
	  if (mmenu) {
	  	ShowMenu_t ShowMenu = (ShowMenu_t)dlsym(mmenu, "ShowMenu");
		MenuReturnStatus status = ShowMenu(rom_path, save_template_path);

	  	if (status==kStatusExitGame) {
			should_quit = 1;
			plat_video_menu_leave();
	  	}
	  	else if (status==kStatusOpenMenu) {
	  		menu_loop();
	  	}
	  	else if (status>=kStatusLoadSlot) {
	  		state_slot = status - kStatusLoadSlot;
			load_state_file(state_slot);
	  	}
	  	else if (status>=kStatusSaveSlot) {
	  		state_slot = status - kStatusSaveSlot;
			save_state_file(state_slot);
	  	}
		
		// release that menu key
		SDL_Event sdlevent;
		sdlevent.type = SDL_KEYUP;
		sdlevent.key.keysym.sym = SDLK_ESCAPE;
		SDL_PushEvent(&sdlevent);
	  }
	  else {
	  	menu_loop();
	  }
      break;
    default:
      break;
  }

  prev_action = action;
}

void synchronize(void)
{
  static uint32_t vsyncs = 0;
  static uint32_t renders = 0;
  static uint32_t nextsec = 0;
  static uint32_t skipped_frames = 0;
  unsigned int ticks = 0;

  float capacity = plat_sound_capacity();

  switch (frameskip_style)
  {
    case FRAMESKIP_AUTO:
      skip_next_frame = 0;

      if (capacity > FRAMESKIP_UNDERRUN_THRESHOLD) {
        skip_next_frame = 1;
        skipped_frames++;
      }
      break;
    case FRAMESKIP_MANUAL:
      skip_next_frame = 1;
      skipped_frames++;
      break;
    default:
      skip_next_frame = 0;
      break;
  }

  if (skipped_frames > max_frameskip) {
    skip_next_frame = 0;
    skipped_frames = 0;
  }

  if (show_fps) {
    ticks = plat_get_ticks_ms();
    if (ticks > nextsec) {
      vsyncsps = vsyncs;
      rendersps = renders;
      vsyncs = 0;
      renders = 0;
      nextsec = ticks + 1000;
    }
    vsyncs++;
    if (!skip_next_frame) renders++;
  }
}

void print_hud()
{
  char msg[HUD_LEN];
  if (show_fps) {
    snprintf(msg, HUD_LEN, "FPS: %2.0f (%4.1f)", rendersps, vsyncsps);
    plat_video_set_msg(msg);
  }
}

int save_state_file(unsigned state_slot)
{
  char state_filename[MAXPATHLEN];
  void *data;
  FILE *f;
  int ret = 0;
  state_file_name(state_filename, MAXPATHLEN, state_slot);

  f = fopen(state_filename, "wb");

  if (!f)
    return -1;

  data = calloc(1, GBA_STATE_MEM_SIZE);
  if (!data) {
    ret = -1;
    goto fail;
  }

  gba_save_state(data);

  if (fwrite(data, 1, GBA_STATE_MEM_SIZE, f) != GBA_STATE_MEM_SIZE) {
    ret = -1;
    goto fail;
  }

fail:
  if (data)
    free(data);
  if (f)
    fclose(f);

  sync();
  return ret;
}

int load_state_file(unsigned state_slot)
{
  char state_filename[MAXPATHLEN];
  void *data;
  FILE *f;
  int ret = 0;
  state_file_name(state_filename, MAXPATHLEN, state_slot);

  f = fopen(state_filename, "rb");

  if (!f)
    return -1;

  data = calloc(1, GBA_STATE_MEM_SIZE);
  if (!data) {
    ret = -1;
    goto fail;
  }


  if (fread(data, 1, GBA_STATE_MEM_SIZE, f) != GBA_STATE_MEM_SIZE) {
    ret = -1;
    goto fail;
  }

  gba_load_state(data);

fail:
  if (data)
    free(data);
  if (f)
    fclose(f);

  return ret;
}

int main(int argc, char *argv[])
{
  bool bios_loaded = false;
  char bios_filename[MAXPATHLEN];
  char filename[MAXPATHLEN];
  char path[MAXPATHLEN];

  if (argc < 2) {
    printf("Usage: picogpsp FILE");
    return 0;
  };

  strncpy(filename, argv[1], MAXPATHLEN);
  if (filename[0] != '/') {
    getcwd(path, MAXPATHLEN);
    if (strlen(path) + strlen(filename) + 1 < MAXPATHLEN) {
      strcat(path, "/");
      strcat(path, filename);
      strcpy(filename, path);
    } else
      filename[0] = 0;
  }

  if (selected_bios == auto_detect || selected_bios == official_bios) {
    bios_loaded = true;
    // getcwd(bios_filename, MAXPATHLEN);
    // strncat(bios_filename, "/gba_bios.bin", MAXPATHLEN - strlen(bios_filename));
	sprintf(bios_filename, "%s/Bios/GBA/gba_bios.bin", getenv("SDCARD_PATH"));

    if (load_bios(bios_filename)) {
      if (selected_bios == official_bios)
        printf("Could not load BIOS image file\n");
      bios_loaded = false;
    }

    if (bios_loaded && bios_rom[0] != 0x18) {
      if (selected_bios == official_bios)
        printf("BIOS image seems incorrect\n");
      bios_loaded = false;
    }
  }

  if (bios_loaded) {
    printf("Using official BIOS\n");
  } else {
    /* Load the built-in BIOS */
    memcpy(bios_rom, open_gba_bios_rom, sizeof(bios_rom));
    printf("Using built-in BIOS\n");
  }

  getcwd(main_path, 512);
  sprintf(save_path, "%s/Saves/GBA", getenv("SDCARD_PATH"));
  // plat_get_root_dir(save_path, 512);

  if (!gamepak_rom)
    init_gamepak_buffer();

  if(!gba_screen_pixels)
    gba_screen_pixels = (uint16_t*)calloc(GBA_SCREEN_PITCH * GBA_SCREEN_HEIGHT, sizeof(uint16_t));

  if (plat_init()) {
    return -1;
  };

  if (load_gamepak(filename) != 0)
  {
    fprintf(stderr, "Could not load the game file.\n");
    return -1;
  }

	gamepak_related_name(save_template_path, MAXPATHLEN, ".st%i");

  init_main();
  init_sound(1);
  menu_init();

#if defined(HAVE_DYNAREC)
  if (dynarec_enable)
  {
#ifdef HAVE_MMAP
    rom_translation_cache = mmap(NULL, ROM_TRANSLATION_CACHE_SIZE,
                                 PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    ram_translation_cache = mmap(NULL, RAM_TRANSLATION_CACHE_SIZE,
                                 PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    bios_translation_cache = mmap(NULL, BIOS_TRANSLATION_CACHE_SIZE,
                                 PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
  }
  else
    dynarec_enable = 0;
#else
  dynarec_enable = 0;
#endif

  reset_gba();
  
	mmenu = dlopen("libmmenu.so", RTLD_LAZY);
	if (mmenu) {
		ResumeSlot_t ResumeSlot = (ResumeSlot_t)dlsym(mmenu, "ResumeSlot");
		if (ResumeSlot) resume_slot = ResumeSlot();
	}
	strcpy(rom_path, filename);
  
  if (resume_slot!=-1) {
	state_slot = resume_slot;
	load_state_file(state_slot);
	resume_slot = -1;
  }

  do {
    update_input(); if (should_quit) break;

    synchronize();

#ifdef HAVE_DYNAREC
    if (dynarec_enable)
      execute_arm_translate(execute_cycles);
    else
#endif
      execute_arm(execute_cycles);

    render_audio();

    print_hud();

    if (!skip_next_frame)
      plat_video_flip();
  } while (!should_quit);

  quit();
  return 0;
}

void quit()
{
  update_backup();

  memory_term();

  if (gba_screen_pixels_prev) {
    free(gba_screen_pixels_prev);
    gba_screen_pixels_prev = NULL;
  }

  if (gba_processed_pixels) {
    free(gba_processed_pixels);
    gba_processed_pixels = NULL;
  }

  free(gba_screen_pixels);
  gba_screen_pixels = NULL;

#if defined(HAVE_MMAP) && defined(HAVE_DYNAREC)
  munmap(rom_translation_cache, ROM_TRANSLATION_CACHE_SIZE);
  munmap(ram_translation_cache, RAM_TRANSLATION_CACHE_SIZE);
  munmap(bios_translation_cache, BIOS_TRANSLATION_CACHE_SIZE);
#endif

  menu_finish();
  plat_finish();

  exit(0);
}
