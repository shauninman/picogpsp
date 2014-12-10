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

#ifndef VIDEO_H
#define VIDEO_H

void update_scanline();
void init_video();
void video_resolution_large();
void video_resolution_small();
void clear_screen(u16 color);
void video_write_savestate(void);
void video_read_savestate(void);

extern u32 frame_speed;

extern u32 resolution_width, resolution_height;

extern s32 affine_reference_x[2];
extern s32 affine_reference_y[2];

typedef void (* tile_render_function)(u32 layer_number, u32 start, u32 end,
 void *dest_ptr);
typedef void (* bitmap_render_function)(u32 start, u32 end, void *dest_ptr);

typedef struct
{
  tile_render_function normal_render_base;
  tile_render_function normal_render_transparent;
  tile_render_function alpha_render_base;
  tile_render_function alpha_render_transparent;
  tile_render_function color16_render_base;
  tile_render_function color16_render_transparent;
  tile_render_function color32_render_base;
  tile_render_function color32_render_transparent;
} tile_layer_render_struct;

typedef struct
{
  bitmap_render_function normal_render;
} bitmap_layer_render_struct;

typedef enum
{
  unscaled,
  scaled_aspect,
  fullscreen,
} video_scale_type;

typedef enum
{
  filter_nearest,
  filter_bilinear
} video_filter_type;

extern video_scale_type screen_scale;
extern video_scale_type current_scale;
extern video_filter_type screen_filter;

void set_gba_resolution(video_scale_type scale);

#ifdef __LIBRETRO__
extern u16 gba_screen_pixels[GBA_SCREEN_PITCH * GBA_SCREEN_HEIGHT];
#endif

#endif
