#ifndef __FRONTEND_PLAT_H__
#define __FRONTEND_PLAT_H__

#define HUD_LEN 39

int  plat_init(void);
void plat_finish(void);
void plat_minimize(void);
void *plat_prepare_screenshot(int *w, int *h, int *bpp);

void plat_video_open(void);
void plat_video_set_msg(char *new_msg);
void plat_video_flip(void);
void plat_video_close(void);

float plat_sound_capacity(void);
void plat_sound_write(void *data, int bytes);

#endif /* __FRONTEND_PLAT_H__ */
