#ifndef __FRONTEND_SCALE_H__
#define __FRONTEND_SCALE_H__

void scale2x(uint16_t* dst, uint16_t* src);

void video_post_process(void);
void video_scale(uint16_t *dst, uint32_t dst_h, uint32_t dst_pitch);
void video_clear_msg(uint16_t *dst, uint32_t dst_h, uint32_t dst_pitch);
void video_print_msg(uint16_t *dst, uint32_t dst_h, uint32_t dst_pitch, char *msg);

#endif /* __FRONTEND_SCALE_H__ */
