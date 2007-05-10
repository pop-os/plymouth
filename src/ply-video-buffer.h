/* vim: ts=4 sw=2 expandtab autoindent cindent 
 * ply-video-buffer.h - framebuffer abstraction
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
#ifndef PLY_VIDEO_BUFFER_H
#define PLY_VIDEO_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _PlyVideoBuffer PlyVideoBuffer;
typedef struct _PlyVideoBufferArea PlyVideoBufferArea;

struct _PlyVideoBufferArea
{
  unsigned long x;
  unsigned long y;
  unsigned long width;
  unsigned long height;
};

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
PlyVideoBuffer *ply_video_buffer_new (const char *device_name);
void ply_video_buffer_free (PlyVideoBuffer *buffer);
bool ply_video_buffer_open (PlyVideoBuffer *buffer);
bool ply_video_buffer_device_is_open (PlyVideoBuffer *buffer); 
char *ply_video_buffer_get_device_name (PlyVideoBuffer *buffer);
void ply_video_buffer_set_device_name (PlyVideoBuffer *buffer,
                                       const char     *device_name);
void ply_video_buffer_close (PlyVideoBuffer *buffer);
void ply_video_buffer_get_size (PlyVideoBuffer     *buffer,
                                PlyVideoBufferArea *size);
bool ply_video_buffer_fill_with_color (PlyVideoBuffer      *buffer,
                                       PlyVideoBufferArea  *area,
                                       double               red, 
                                       double               green,
                                       double               blue, 
                                       double               alpha);

bool ply_video_buffer_fill_with_argb32_data (PlyVideoBuffer      *buffer,
                                             PlyVideoBufferArea  *area,
                                             unsigned long        x,
                                             unsigned long        y,
                                             unsigned long        width,
                                             unsigned long        height,
                                             uint32_t            *data);


#endif

#endif /* PLY_VIDEO_BUFFER_H */
