#include <sys/stat.h>
#include "common.h"

#include "frontend/config.h"
#include "frontend/main.h"
#include "frontend/menu.h"
#include "frontend/scale.h"
#include "frontend/plat.h"

#include "frontend/libpicofe/config_file.h"

#define MENU_ALIGN_LEFT 0
#define MENU_X2 0

typedef enum
{
  MA_NONE = 1,
  MA_MAIN_RESUME_GAME,
  MA_MAIN_SAVE_STATE,
  MA_MAIN_LOAD_STATE,
  MA_MAIN_RESET_GAME,
  MA_MAIN_CREDITS,
  MA_MAIN_EXIT,
  MA_OPT_SAVECFG,
  MA_OPT_SAVECFG_GAME,
  MA_CTRL_PLAYER1,
  MA_CTRL_EMU,
} menu_id;

int menu_save_config(int is_game);
void menu_set_defaults(void);

me_bind_action me_ctrl_actions[] =
{
  { "UP       ", 1 << KBIT_UP},
  { "DOWN     ", 1 << KBIT_DOWN },
  { "LEFT     ", 1 << KBIT_LEFT },
  { "RIGHT    ", 1 << KBIT_RIGHT },
  { "A BUTTON ", 1 << KBIT_A },
  { "B BUTTON ", 1 << KBIT_B },
  { "START    ", 1 << KBIT_START },
  { "SELECT   ", 1 << KBIT_SELECT },
  { "L BUTTON ", 1 << KBIT_L },
  { "R BUTTON ", 1 << KBIT_R },
  { NULL,       0 }
};

me_bind_action emuctrl_actions[] =
{
  { "Save State       ", 1 << EACTION_SAVE_STATE },
  { "Load State       ", 1 << EACTION_LOAD_STATE },
  { "Toggle Frameskip ", 1 << EACTION_TOGGLE_FSKIP },
  { "Show/Hide FPS    ", 1 << EACTION_TOGGLE_FPS },
  { "Toggle FF        ", 1 << EACTION_TOGGLE_FF },
  // { "Enter Menu       ", 1 << EACTION_MENU }, // TRIMUI
  { NULL,                0 }
};

int emu_check_save_file(int slot, int *time)
{
  char fname[MAXPATHLEN];
  struct stat status;
  int ret;

  state_file_name(fname, sizeof(fname), slot);

  ret = stat(fname, &status);
  if (ret != 0)
    return 0;

  return 1;
}

int emu_save_load_game(int load, int unused)
{
  int ret;

  if (load)
    ret = load_state_file(state_slot);
  else
    ret = save_state_file(state_slot);

  return ret;
}

#include "frontend/libpicofe/menu.c"

static const char *mgn_saveloadcfg(int id, int *offs)
{
  return "";
}

static int mh_restore_defaults(int id, int keys)
{
  menu_set_defaults();
  menu_update_msg("defaults restored");
  return 1;
}

static int mh_savecfg(int id, int keys)
{
  if (menu_save_config(id == MA_OPT_SAVECFG_GAME ? 1 : 0) == 0)
    menu_update_msg("config saved");
  else
    menu_update_msg("failed to write config");

  return 1;
}

static const char h_restore_def[]     = "Switches back to default / recommended\n"
          "configuration";

static const char h_color_correct[]   = "Modifies colors to simulate original display";
static const char h_lcd_blend[]       = "Blends frames to simulate LCD lag";
static const char h_show_fps[]        = "Shows frames and vsyncs per second";
static const char h_dynarec_enable[]  = "Improves performance, but may reduce accuracy";


static const char *men_frameskip[] = { "OFF", "Auto", "Manual", NULL };

static const char *men_scaling[] = { "Native", "3:2 Sharp", "3:2 Smooth", "4:3 Sharp", "4:3 Smooth", NULL};

static menu_entry e_menu_options[] =
{
  mee_enum         ("Frameskip",                0, frameskip_style, men_frameskip),
  mee_range        ("Max Frameskip",            0, max_frameskip, 1, 5),
  mee_enum         ("Scaling",                  0, scaling_mode, men_scaling),
  mee_onoff_h      ("Color Correction",         0, color_correct, 1, h_color_correct),
  mee_onoff_h      ("LCD Ghosting",             0, lcd_blend, 1, h_lcd_blend),
  mee_onoff_h      ("Dynamic Recompiler",       0, dynarec_enable, 1, h_dynarec_enable),
  mee_onoff_h      ("Show FPS",                 0, show_fps, 1, h_show_fps),
  mee_cust_nosave  ("Save global config",       MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
  mee_cust_nosave  ("Save game config",         MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
  mee_handler_h    ("Restore defaults",         mh_restore_defaults, h_restore_def),
  mee_end,
};

static int menu_loop_options(int id, int keys)
{
  static int sel = 0;
  int prev_dynarec_enable = dynarec_enable;

  me_loop(e_menu_options, &sel);

  if (prev_dynarec_enable != dynarec_enable)
    init_caches();

  return 0;
}

static int key_config_loop_wrap(int id, int keys)
{
  switch (id) {
    case MA_CTRL_PLAYER1:
      key_config_loop(me_ctrl_actions, array_size(me_ctrl_actions) - 1, 0);
      break;
    case MA_CTRL_EMU:
      key_config_loop(emuctrl_actions, array_size(emuctrl_actions) - 1, -1);
      break;
    default:
      break;
  }
  return 0;
}

static menu_entry e_menu_keyconfig[] =
{
  mee_handler_id  ("Player controls",    MA_CTRL_PLAYER1,     key_config_loop_wrap),
  mee_handler_id  ("Emulator controls",  MA_CTRL_EMU,         key_config_loop_wrap),
  mee_cust_nosave ("Save global config", MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
  mee_cust_nosave ("Save game config",   MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
  mee_end,
};

static int menu_loop_keyconfig(int id, int keys)
{
  static int sel = 0;
  me_loop(e_menu_keyconfig, &sel);
  return 0;
}


static int main_menu_handler(int id, int keys)
{
  switch (id)
  {
  case MA_MAIN_RESUME_GAME:
    return 1;
  case MA_MAIN_SAVE_STATE:
    return menu_loop_savestate(0);
  case MA_MAIN_LOAD_STATE:
    return menu_loop_savestate(1);
  case MA_MAIN_RESET_GAME:
    reset_gba();
    return 1;
  case MA_MAIN_EXIT:
    should_quit = 1;
    return 1;
  default:
    lprintf("%s: something unknown selected\n", __FUNCTION__);
    break;
  }

  return 0;
}

static menu_entry e_menu_main[] =
{
  mee_handler_id("Resume game",        MA_MAIN_RESUME_GAME, main_menu_handler),
  mee_handler_id("Save State",         MA_MAIN_SAVE_STATE,  main_menu_handler),
  mee_handler_id("Load State",         MA_MAIN_LOAD_STATE,  main_menu_handler),
  mee_handler_id("Reset game",         MA_MAIN_RESET_GAME,  main_menu_handler),
  mee_handler   ("Options",            menu_loop_options),
  mee_handler   ("Controls",           menu_loop_keyconfig),
  /* mee_handler_id("Cheats",             MA_MAIN_CHEATS,      main_menu_handler), */
  mee_handler_id("Exit",               MA_MAIN_EXIT,        main_menu_handler),
  mee_end,
};

void draw_savestate_bg(int slot)
{
}

void menu_set_defaults(void)
{
  dynarec_enable = 1;
  frameskip_style = 1;
  scaling_mode = SCALING_ASPECT_SMOOTH;
  max_frameskip = 3;
  color_correct = 0;
  lcd_blend = 0;
  show_fps = 0;
  limit_frames = 1;
}

void menu_loop(void)
{
  static int sel = 0;
  plat_video_menu_enter(1);
  video_scale(g_menubg_ptr, g_menuscreen_h, g_menuscreen_pp);
  menu_darken_bg(g_menubg_ptr, g_menubg_ptr, g_menuscreen_h * g_menuscreen_pp, 0);
  me_loop_d(e_menu_main, &sel, NULL, NULL);

  /* wait until menu, ok, back is released */
  while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
    ;
  memset(g_menubg_ptr, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));

  plat_video_menu_leave();
}

int menu_save_config(int is_game)
{
  char config_filename[MAXPATHLEN];
  FILE *config_file;

  config_file_name(config_filename, MAXPATHLEN, is_game);
  config_file = fopen(config_filename, "wb");
  if (!config_file) {
    fprintf(stderr, "Could not write config to %s\n", config_filename);
    return -1;
  }

  config_write(config_file);
  config_write_keys(config_file);

  fclose(config_file);
  return 0;
}


void menu_load_config(void)
{
  char config_filename[MAXPATHLEN];
  FILE *config_file;
  size_t length;
  char *config;

  config_file_name(config_filename, MAXPATHLEN, 1);
  config_file = fopen(config_filename, "rb");
  if (!config_file) {
    config_file_name(config_filename, MAXPATHLEN, 0);
    config_file = fopen(config_filename, "rb");
  }

  if (!config_file)
    return;

  fseek(config_file, 0, SEEK_END);
  length = ftell(config_file);
  fseek(config_file, 0, SEEK_SET);

  config = calloc(1, length);

  if (fread(config, 1, length, config_file))
  fclose(config_file);

  config_read(config);
  config_read_keys(config);

  if (config)
    free(config);
}

void menu_init(void)
{
  menu_init_base();

  menu_set_defaults();
  menu_load_config();

  g_menubg_src_ptr = calloc(g_menuscreen_w * g_menuscreen_h * 2, 1);
  g_menubg_ptr = calloc(g_menuscreen_w * g_menuscreen_h * 2, 1);
  if (g_menubg_src_ptr == NULL || g_menubg_ptr == NULL) {
    fprintf(stderr, "OOM\n");
    exit(1);
  }
}

void menu_finish(void)
{
}

static void debug_menu_loop(void)
{
}

void menu_update_msg(const char *msg)
{
  strncpy(menu_error_msg, msg, sizeof(menu_error_msg));
  menu_error_msg[sizeof(menu_error_msg) - 1] = 0;

  menu_error_time = plat_get_ticks_ms();
  lprintf("msg: %s\n", menu_error_msg);
}
