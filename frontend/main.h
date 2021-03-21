#ifndef __FRONTEND_MAIN_H__
#define __FRONTEND_MAIN_H__

#include <stddef.h>

#define MAXPATHLEN 512

typedef enum {
  EACTION_NONE = 0,
  EACTION_MENU,
  EACTION_TOGGLE_FPS,
  EACTION_TOGGLE_FSKIP,
  EACTION_TOGGLE_FF,
  EACTION_SAVE_STATE,
  EACTION_LOAD_STATE,
  EACTION_QUIT,
} emu_action;

typedef enum {
  KBIT_A = 0,
  KBIT_B,
  KBIT_SELECT,
  KBIT_START,
  KBIT_RIGHT,
  KBIT_LEFT,
  KBIT_UP,
  KBIT_DOWN,
  KBIT_R,
  KBIT_L
} keybit;

typedef enum {
  FRAMESKIP_NONE = 0,
  FRAMESKIP_AUTO,
  FRAMESKIP_MANUAL,
} frameskip_style_t;

typedef enum {
  SCALING_NONE = 0,
  SCALING_ASPECT_SHARP,
  SCALING_ASPECT_SMOOTH,
  SCALING_FULL_SHARP,
  SCALING_FULL_SMOOTH,
} scaling_mode_t;


extern int should_quit;

extern int state_slot;
extern int dynarec_enable;
extern frameskip_style_t frameskip_style;
extern scaling_mode_t scaling_mode;
extern int max_frameskip;
extern int color_correct;
extern int lcd_blend;
extern int show_fps;
extern int limit_frames;

extern uint16_t *gba_screen_pixels_prev;
extern uint16_t *gba_processed_pixels;

#define array_size(x) (sizeof(x) / sizeof(x[0]))

void state_file_name(char *buf, size_t len, unsigned state_slot);
void config_file_name(char *buf, size_t len, int is_game);
void handle_emu_action(emu_action action);
int save_state_file(unsigned state_slot);
int load_state_file(unsigned state_slot);

#endif /* __FRONTEND_MAIN_H__ */
