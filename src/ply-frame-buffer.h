/* vim: ts=4 sw=2 expandtab autoindent cindent 
 * ply-frame-buffer.h - framebuffer abstraction
 *
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_FRAME_BUFFER_H
#define PLY_FRAME_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "ply-utils.h"

typedef struct _PlyFrameBuffer PlyFrameBuffer;
typedef struct _PlyFrameBufferArea PlyFrameBufferArea;

struct _PlyFrameBufferArea
{
  unsigned long x;
  unsigned long y;
  unsigned long width;
  unsigned long height;
};

#define PLY_FRAME_BUFFER_COLOR_TO_PIXEL_VALUE(r,g,b,a)                        \
    (((uint8_t) (CLAMP (a * 255.0, 0.0, 255.0)) << 24)                        \
      | ((uint8_t) (CLAMP (r * 255.0, 0.0, 255.0)) << 16)                     \
      | ((uint8_t) (CLAMP (g * 255.0, 0.0, 255.0)) << 8)                      \
      | ((uint8_t) (CLAMP (b * 255.0, 0.0, 255.0))))

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
PlyFrameBuffer *ply_frame_buffer_new (const char *device_name);
void ply_frame_buffer_free (PlyFrameBuffer *buffer);
bool ply_frame_buffer_open (PlyFrameBuffer *buffer);
void ply_frame_buffer_pause_updates (PlyFrameBuffer *buffer);
bool ply_frame_buffer_unpause_updates (PlyFrameBuffer *buffer);
bool ply_frame_buffer_device_is_open (PlyFrameBuffer *buffer); 
char *ply_frame_buffer_get_device_name (PlyFrameBuffer *buffer);
void ply_frame_buffer_set_device_name (PlyFrameBuffer *buffer,
                                       const char     *device_name);
void ply_frame_buffer_close (PlyFrameBuffer *buffer);
void ply_frame_buffer_get_size (PlyFrameBuffer     *buffer,
                                PlyFrameBufferArea *size);
bool ply_frame_buffer_fill_with_color (PlyFrameBuffer      *buffer,
                                       PlyFrameBufferArea  *area,
                                       double               red, 
                                       double               green,
                                       double               blue, 
                                       double               alpha);

bool ply_frame_buffer_fill_with_argb32_data (PlyFrameBuffer      *buffer,
                                             PlyFrameBufferArea  *area,
                                             unsigned long        x,
                                             unsigned long        y,
                                             unsigned long        width,
                                             unsigned long        height,
                                             uint32_t            *data);
bool ply_frame_buffer_fill_with_argb32_data_at_opacity (PlyFrameBuffer     *buffer,
                                                        PlyFrameBufferArea *area,
                                                        unsigned long       x,
                                                        unsigned long       y,
                                                        unsigned long       width,
                                                        unsigned long       height,
                                                        uint32_t           *data,
                                                        double              opacity);

#endif

#endif /* PLY_FRAME_BUFFER_H */
