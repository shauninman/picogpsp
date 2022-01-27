#include <SDL/SDL.h>
#include "common.h"

#include "frontend/main.h"
#include "frontend/plat.h"
#include "frontend/scale.h"
#include "frontend/libpicofe/menu.h"
#include "frontend/libpicofe/plat.h"
#include "frontend/libpicofe/input.h"
#include "frontend/libpicofe/in_sdl.h"

static SDL_Surface* screen;

#define BUF_LEN 8192

static short buf[BUF_LEN];
static int buf_w, buf_r;

static char msg[HUD_LEN];

static const struct in_default_bind in_sdl_defbinds[] = {
  { SDLK_UP,        IN_BINDTYPE_PLAYER12, KBIT_UP },
  { SDLK_DOWN,      IN_BINDTYPE_PLAYER12, KBIT_DOWN },
  { SDLK_LEFT,      IN_BINDTYPE_PLAYER12, KBIT_LEFT },
  { SDLK_RIGHT,     IN_BINDTYPE_PLAYER12, KBIT_RIGHT },
  { SDLK_LCTRL,     IN_BINDTYPE_PLAYER12, KBIT_B },
  { SDLK_SPACE,     IN_BINDTYPE_PLAYER12, KBIT_A },
  { SDLK_RETURN,    IN_BINDTYPE_PLAYER12, KBIT_START },
  { SDLK_RCTRL,     IN_BINDTYPE_PLAYER12, KBIT_SELECT },
  { SDLK_e,         IN_BINDTYPE_PLAYER12, KBIT_L },
  { SDLK_t,         IN_BINDTYPE_PLAYER12, KBIT_R },
  { SDLK_ESCAPE,    IN_BINDTYPE_EMU, EACTION_MENU },
  { 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] =
{
  { SDLK_UP,        PBTN_UP },
  { SDLK_DOWN,      PBTN_DOWN },
  { SDLK_LEFT,      PBTN_LEFT },
  { SDLK_RIGHT,     PBTN_RIGHT },
  { SDLK_SPACE,     PBTN_MOK },
  { SDLK_LCTRL,     PBTN_MBACK },
  { SDLK_LALT,      PBTN_MA2 },
  { SDLK_LSHIFT,    PBTN_MA3 },
  { SDLK_TAB,       PBTN_L },
  { SDLK_BACKSPACE, PBTN_R },
};

const struct menu_keymap in_sdl_joy_map[] =
{
  { SDLK_UP,    PBTN_UP },
  { SDLK_DOWN,  PBTN_DOWN },
  { SDLK_LEFT,  PBTN_LEFT },
  { SDLK_RIGHT, PBTN_RIGHT },
  { SDLK_WORLD_0, PBTN_MOK },
  { SDLK_WORLD_1, PBTN_MBACK },
  { SDLK_WORLD_2, PBTN_MA2 },
  { SDLK_WORLD_3, PBTN_MA3 },
};

static const char * const in_sdl_key_names[SDLK_LAST] = {
  [SDLK_UP]         = "up",
  [SDLK_DOWN]       = "down",
  [SDLK_LEFT]       = "left",
  [SDLK_RIGHT]      = "right",
  [SDLK_LSHIFT]     = "x",
  [SDLK_LCTRL]      = "b",
  [SDLK_SPACE]      = "a",
  [SDLK_LALT]       = "y",
  [SDLK_RETURN]     = "start",
  [SDLK_RCTRL]      = "select",
  [SDLK_e]          = "l",
  [SDLK_t]          = "r",
  [SDLK_ESCAPE]     = "menu",
};

static const struct in_pdata in_sdl_platform_data = {
  .defbinds  = in_sdl_defbinds,
  .key_map   = in_sdl_key_map,
  .kmap_size = array_size(in_sdl_key_map),
  .joy_map   = in_sdl_joy_map,
  .jmap_size = array_size(in_sdl_joy_map),
  .key_names = in_sdl_key_names,
};

static void *fb_flip(void)
{
  SDL_Flip(screen);
  return screen->pixels;
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
  g_menuscreen_ptr = screen->pixels; // fb_flip();
}

void plat_video_menu_end(void)
{
  g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_leave(void)
{
  SDL_LockSurface(screen);
  memset(g_menuscreen_ptr, 0, g_menuscreen_w * g_menuscreen_h * sizeof(uint16_t));
  SDL_UnlockSurface(screen);
  g_menuscreen_ptr = fb_flip();
  SDL_LockSurface(screen);
  memset(g_menuscreen_ptr, 0, g_menuscreen_w * g_menuscreen_h * sizeof(uint16_t));
  SDL_UnlockSurface(screen);
}

void plat_video_open(void)
{
}

void plat_video_set_msg(char *new_msg)
{
  snprintf(msg, HUD_LEN, "%s", new_msg);
}

void plat_video_flip(void)
{
  video_post_process();
  SDL_LockSurface(screen);
  if (msg[0])
    video_clear_msg(screen->pixels, screen->h, screen->pitch / sizeof(uint16_t));

  video_scale(screen->pixels, screen->h, screen->pitch / sizeof(uint16_t));

  if (msg[0])
    video_print_msg(screen->pixels, screen->h, screen->pitch / sizeof(uint16_t), msg);
  SDL_UnlockSurface(screen);

  g_menuscreen_ptr = fb_flip();
  msg[0] = 0;
}

void plat_video_close(void)
{
}

void plat_sound_callback(void *unused, u8 *stream, int len)
{
  short *p = (short *)stream;
  len /= sizeof(short);

  while (buf_r != buf_w && len > 0) {
    *p++ = buf[buf_r++];
    buf_r %= BUF_LEN;
    --len;
  }

  while(len > 0) {
    *p++ = 0;
    --len;
  }
}

void plat_sound_finish(void)
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

int plat_sound_init(void)
{

  if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return -1;
  }

  SDL_AudioSpec spec;

  spec.freq = sound_frequency;
  spec.format = AUDIO_S16SYS;
  spec.channels = 2;
  spec.samples = 512;
  spec.callback = plat_sound_callback;

  if (SDL_OpenAudio(&spec, NULL) < 0) {
    plat_sound_finish();
    return -1;
  }

  SDL_PauseAudio(0);
  return 0;
}

float plat_sound_capacity(void)
{
  int buffered = 0;
  if (buf_w != buf_r) {
    buffered = buf_w > buf_r ? buf_w - buf_r : (buf_w + BUF_LEN) - buf_r;
  }

  return 1.0 - (float)buffered / BUF_LEN;
}

void plat_sound_write(void *data, int bytes)
{
  short *sound_data = (short *)data;
  SDL_LockAudio();

  while (bytes > 0) {
    while (((buf_w + 1) % BUF_LEN) == buf_r) {
      SDL_UnlockAudio();

      if (!limit_frames)
        return;

      plat_sleep_ms(1);
      SDL_LockAudio();
    }
    buf[buf_w] = *sound_data++;

    ++buf_w;
    buf_w %= BUF_LEN;
    bytes -= sizeof(short);
  }
  SDL_UnlockAudio();
}

void plat_sdl_event_handler(void *event_)
{
}

int plat_init(void)
{
  SDL_Init(SDL_INIT_VIDEO);
  screen = SDL_SetVideoMode(640, 480, 16, SDL_HWSURFACE);

  if (screen == NULL) {
    printf("%s, failed to set video mode\n", __func__);
    return -1;
  }

  SDL_ShowCursor(0);

  g_menuscreen_w = 640;
  g_menuscreen_h = 480;
  g_menuscreen_pp = 640;
  g_menuscreen_ptr = fb_flip();

  if (in_sdl_init(&in_sdl_platform_data, plat_sdl_event_handler)) {
    fprintf(stderr, "SDL input failed to init: %s\n", SDL_GetError());
    return -1;
  }
  in_probe();

  sound_frequency = 48000;

  if (plat_sound_init()) {
    fprintf(stderr, "SDL sound failed to init: %s\n", SDL_GetError());
    return -1;
  }
  return 0;
}

void plat_pre_finish(void)
{
}

void plat_finish(void)
{
  plat_sound_finish();
  SDL_Quit();
}

void plat_trigger_vibrate(int pad, int low, int high)
{
}
