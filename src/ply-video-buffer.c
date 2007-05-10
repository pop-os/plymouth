/* vim: ts=4 sw=2 expandtab autoindent cindent 
 * ply-video-buffer.c - framebuffer abstraction
 *
 * Copyright (C) 2006, 2007 Red Hat, Inc.
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
 * Written by: Kristian Høgsberg <krh@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include "ply-video-buffer.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/fb.h>

#ifndef PLY_VIDEO_BUFFER_DEFAULT_FB_DEVICE_NAME
#define PLY_VIDEO_BUFFER_DEFAULT_FB_DEVICE_NAME "/dev/fb"
#endif

#define MIN(a,b) (a <= b? a : b)
#define MAX(a,b) (a >= b? a : b)
#define CLAMP(a,b,c) (((a) > (c)) ? (c) : (((a) < (b)) ? (b) : (a)))

typedef union 
{
  uint16_t *for_16bpp;
  uint32_t *for_32bpp;
  char *address;
} PlyVideoBufferPixelLayout;

struct _PlyVideoBuffer
{
  char  *device_name;
  int    device_fd;

  PlyVideoBufferPixelLayout layout;
  size_t layout_size;

  PlyVideoBufferPixelLayout shadow_layout;

  unsigned int bits_per_pixel;
  unsigned int bytes_per_pixel;
  PlyVideoBufferArea area;
  PlyVideoBufferArea area_to_flush;
};

static bool ply_video_buffer_open_device (PlyVideoBuffer  *buffer);
static void ply_video_buffer_close_device (PlyVideoBuffer *buffer);
static bool ply_video_buffer_query_device (PlyVideoBuffer *buffer);
static bool ply_video_buffer_map_to_layout (PlyVideoBuffer *buffer);
static uint32_t ply_video_buffer_convert_color_to_pixel_value (
    PlyVideoBuffer *buffer,
    uint8_t         red, 
    uint8_t         green,
    uint8_t         blue, 
    uint8_t         alpha);
static void ply_video_buffer_set_pixel_to_value (PlyVideoBuffer *buffer,
                                                 int             x,
                                                 int             y,
                                                 uint32_t        pixel_value);

static void ply_video_buffer_add_area_to_flush_area (PlyVideoBuffer     *buffer, 
                                                     PlyVideoBufferArea *area);
static bool ply_video_buffer_flush (PlyVideoBuffer *buffer);

static bool
ply_video_buffer_open_device (PlyVideoBuffer  *buffer)
{
  assert (buffer != NULL);
  assert (buffer->device_name != NULL);

  buffer->device_fd = open (buffer->device_name, O_RDWR);

  if (buffer->device_fd < 0)
    {
      return false;
    }

  return true;
}

static void
ply_video_buffer_close_device (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);

  if (buffer->layout.address != MAP_FAILED)
    {
      munmap (buffer->layout.address, buffer->layout_size);
      buffer->layout.address = MAP_FAILED;
    }

  if (buffer->device_fd >= 0)
    {
      close (buffer->device_fd);
      buffer->device_fd = -1;
    }
}

static bool 
ply_video_buffer_query_device (PlyVideoBuffer *buffer)
{
  struct fb_var_screeninfo variable_screen_info;
  struct fb_fix_screeninfo fixed_screen_info;
  size_t bytes_per_row;

  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);

  if (ioctl (buffer->device_fd, FBIOGET_VSCREENINFO, &variable_screen_info) < 0)
    {
      return false;
    }

  buffer->bits_per_pixel = variable_screen_info.bits_per_pixel;
  buffer->area.x = variable_screen_info.xoffset;
  buffer->area.y = variable_screen_info.yoffset;
  buffer->area.width = variable_screen_info.xres;
  buffer->area.height = variable_screen_info.yres;

  if (ioctl(buffer->device_fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0) 
    {
      return false;
    }

  bytes_per_row = fixed_screen_info.line_length;
  buffer->layout_size = buffer->area.height * bytes_per_row;
  buffer->bytes_per_pixel = bytes_per_row / buffer->area.width;

  return true;
}

static bool
ply_video_buffer_map_to_layout (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);
  assert (buffer->layout_size > 0);

  buffer->layout.address = mmap (NULL, buffer->layout_size, PROT_WRITE,
                                     MAP_SHARED, buffer->device_fd, 0);

  return buffer->layout.address != MAP_FAILED;
}

static uint32_t
ply_video_buffer_convert_color_to_pixel_value (PlyVideoBuffer *buffer,
                                               uint8_t         red, 
                                               uint8_t         green,
                                               uint8_t         blue, 
                                               uint8_t         alpha)
{
  uint32_t pixel_value;

  assert (buffer != NULL);

  switch (buffer->bytes_per_pixel)
    {
      case 2:

        red >>= 3;
        green >>= 2;
        blue >>= 3;

        pixel_value = (red << 11) | (green << 5) | blue;
        break;

      case 3:
        pixel_value = (red << 16) | (green << 8) | blue;
        break;

      case 4:
        pixel_value = (alpha << 24) | (red << 16) | (green << 8) | blue;
        break;

      default:
        assert ((buffer->bytes_per_pixel == 2) 
                || (buffer->bytes_per_pixel == 3) 
                || (buffer->bytes_per_pixel == 4));
        break;
    }

  return pixel_value;
}

static void
ply_video_buffer_set_pixel_to_value (PlyVideoBuffer *buffer,
                                     int             x,
                                     int             y,
                                     uint32_t        pixel_value)
{
  unsigned long bytes_per_row;
  unsigned long offset;

  assert (buffer != NULL);

  switch (buffer->bytes_per_pixel)
    {
      case 2:
        buffer->shadow_layout.for_16bpp[y * buffer->area.width + x] = 
          (uint16_t) pixel_value;
        break;

      case 3:
        bytes_per_row = buffer->bytes_per_pixel * buffer->area.width;
        offset = (y * bytes_per_row) + (x * buffer->bytes_per_pixel);

        /* FIXME: I think we're going to need to byteswap pixel_value on
         * ppc
         */
        memcpy (buffer->shadow_layout.address + offset, &pixel_value,
                buffer->bytes_per_pixel);
        break;

      case 4:
        buffer->shadow_layout.for_32bpp[y * buffer->area.width + x] = pixel_value;
        break;

      default:
        assert ((buffer->bytes_per_pixel == 2) 
                || (buffer->bytes_per_pixel == 3) 
                || (buffer->bytes_per_pixel == 4));
        break;
    }
}

static void
ply_video_buffer_add_area_to_flush_area (PlyVideoBuffer     *buffer, 
                                         PlyVideoBufferArea *area)
{
  assert (buffer != NULL);
  assert (area != NULL);
  assert (area->x >= buffer->area.x);
  assert (area->y >= buffer->area.y);
  assert (area->x < buffer->area.width);
  assert (area->y < buffer->area.height);
  assert (area->width >= 0);
  assert (area->height >= 0);

  buffer->area_to_flush.x = MIN (buffer->area_to_flush.x, area->x);
  buffer->area_to_flush.y = MIN (buffer->area_to_flush.y, area->y);
  buffer->area_to_flush.width = MAX (buffer->area_to_flush.width, area->width);
  buffer->area_to_flush.height = MAX (buffer->area_to_flush.height, area->height);
}

static bool 
ply_video_buffer_flush (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);
  unsigned long bytes_per_row;
  unsigned long start_offset;
  size_t size;

  bytes_per_row = buffer->bytes_per_pixel * buffer->area.width;
  start_offset = (buffer->area_to_flush.y * bytes_per_row)
                 + (buffer->area_to_flush.x * buffer->bytes_per_pixel);
  size = buffer->area_to_flush.width * buffer->area_to_flush.height
         * buffer->bytes_per_pixel; 

  memcpy (buffer->layout.address + start_offset,
          buffer->shadow_layout.address + start_offset, size);
  if (msync (buffer->layout.address + start_offset, size, MS_SYNC) < 0)
    return false;

  buffer->area_to_flush.x = 0; 
  buffer->area_to_flush.y = 0; 
  buffer->area_to_flush.width = 0; 
  buffer->area_to_flush.height = 0; 

  return true;
}

PlyVideoBuffer *
ply_video_buffer_new (const char *device_name)
{
  PlyVideoBuffer *buffer;

  buffer = calloc (1, sizeof (PlyVideoBuffer));

  if (device_name != NULL)
    buffer->device_name = strdup (device_name);
  else
    buffer->device_name = 
      strdup (PLY_VIDEO_BUFFER_DEFAULT_FB_DEVICE_NAME);

  buffer->layout.address = MAP_FAILED;
  buffer->shadow_layout.address = NULL;

  return buffer;
}

void
ply_video_buffer_free (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);

  if (ply_video_buffer_device_is_open (buffer))
    ply_video_buffer_close (buffer);

  free (buffer->device_name);
  free (buffer->shadow_layout.address);
  free (buffer);
}

bool 
ply_video_buffer_open (PlyVideoBuffer *buffer)
{
  bool is_open;

  assert (buffer != NULL);

  is_open = false;

  if (!ply_video_buffer_open_device (buffer))
    {
      goto out;
    }

  if (!ply_video_buffer_query_device (buffer))
    {
      goto out;
    }

  if (!ply_video_buffer_map_to_layout (buffer))
    {
      goto out;
    }

  buffer->shadow_layout.address = 
    realloc (buffer->shadow_layout.address,
             buffer->layout_size);
  memset (buffer->shadow_layout.address, 0, buffer->layout_size);

  is_open = true;

out:

  if (!is_open)
    {
      int saved_errno;

      saved_errno = errno;
      ply_video_buffer_close_device (buffer);
      errno = saved_errno;
    }

  return is_open;
}

bool 
ply_video_buffer_device_is_open (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);
  return buffer->device_fd >= 0 && buffer->layout.address != MAP_FAILED;
}

char *
ply_video_buffer_get_device_name (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);
  assert (ply_video_buffer_device_is_open (buffer));
  assert (buffer->device_name != NULL);

  return strdup (buffer->device_name);
}

void
ply_video_buffer_set_device_name (PlyVideoBuffer *buffer,
                                  const char     *device_name)
{
  assert (buffer != NULL);
  assert (!ply_video_buffer_device_is_open (buffer));
  assert (device_name != NULL);
  assert (buffer->device_name != NULL);

  if (strcmp (buffer->device_name, device_name) != 0)
    {
      free (buffer->device_name);
      buffer->device_name = strdup (device_name);
    }
}

void 
ply_video_buffer_close (PlyVideoBuffer *buffer)
{
  assert (buffer != NULL);

  assert (ply_video_buffer_device_is_open (buffer));
  ply_video_buffer_close_device (buffer);

  buffer->bytes_per_pixel = 0;
  buffer->area.x = 0;
  buffer->area.y = 0;
  buffer->area.width = 0;
  buffer->area.height = 0;
}

void 
ply_video_buffer_get_size (PlyVideoBuffer     *buffer,
                           PlyVideoBufferArea *size)
{
  assert (buffer != NULL);
  assert (ply_video_buffer_device_is_open (buffer));
  assert (size != NULL);

  *size = buffer->area;
}

bool 
ply_video_buffer_fill_with_color (PlyVideoBuffer      *buffer,
                                  PlyVideoBufferArea  *area,
                                  double               red, 
                                  double               green,
                                  double               blue, 
                                  double               alpha)
{
  uint32_t pixel_value;
  long row, column;

  assert (buffer != NULL);
  assert (ply_video_buffer_device_is_open (buffer));

  if (area == NULL)
    area = &buffer->area;

  pixel_value = 
    ply_video_buffer_convert_color_to_pixel_value (buffer, 
                                                   CLAMP (red * 255.0, 0, 255),
                                                   CLAMP (green * 255.0, 0, 255), 
                                                   CLAMP (blue * 255.0, 0, 255),
                                                   CLAMP (alpha * 255.0, 0, 255));
  for (row = 0; row < area->height; row++)
    {
      for (column = 0; column < area->width; column++)
        {
          ply_video_buffer_set_pixel_to_value (buffer, column, row,
                                               pixel_value);
        }
    }

  ply_video_buffer_add_area_to_flush_area (buffer, area);

  return ply_video_buffer_flush (buffer);
}

bool 
ply_video_buffer_fill_with_argb32_data (PlyVideoBuffer     *buffer,
                                        PlyVideoBufferArea *area,
                                        unsigned long       x,
                                        unsigned long       y,
                                        unsigned long       width,
                                        unsigned long       height,
                                        unsigned long       bytes_per_row,
                                        uint32_t           *data)
{
  uint32_t pixel_value;
  long row, column;

  assert (buffer != NULL);
  assert (ply_video_buffer_device_is_open (buffer));
  assert (width * 4 <= bytes_per_row);

  if (area == NULL)
    area = &buffer->area;

  for (row = y; row < height; row++)
    {
      for (column = x; column < width; column++)
        {
          uint8_t red, green, blue, alpha;

          alpha = ((data[bytes_per_row / 4 * row + column] & 0xff000000) >> 24);
          red = ((data[bytes_per_row / 4 * row + column]   & 0x00ff0000) >> 16);
          green = ((data[bytes_per_row / 4 * row + column] & 0x0000ff00) >> 8);
          blue = ((data[bytes_per_row / 4 * row + column]  & 0x000000ff));

          pixel_value = 
            ply_video_buffer_convert_color_to_pixel_value (buffer, red, green,
                                                           blue, alpha);
          ply_video_buffer_set_pixel_to_value (buffer, column, row,
                                               pixel_value);
        }
    }

  ply_video_buffer_add_area_to_flush_area (buffer, area);

  return ply_video_buffer_flush (buffer);
}

#ifdef PLY_VIDEO_BUFFER_ENABLE_TEST

#include <math.h>
#include <stdio.h>
#include <sys/time.h>

static double
get_current_time (void)
{
  const double microseconds_per_second = 1000000.0;
  double timestamp;
  struct timeval now = { 0L, /* zero-filled */ };

  gettimeofday (&now, NULL);
  timestamp = ((microseconds_per_second * now.tv_sec) + now.tv_usec) /
               microseconds_per_second;

  return timestamp;
}

static void
animate_at_time (PlyVideoBuffer *buffer,
                 double          time)
{
  int x, y;
  uint32_t *data;

  data = calloc (1024 * 768, sizeof (uint32_t));

  for (y = 0; y < 768; y++)
    {
      int blue_offset;
      uint8_t red, green, blue, alpha;

      blue_offset = (int) 64 * sin (time) + (255 - 64);
      blue = rand () % blue_offset;
      for (x = 0; x < 1024; x++)
      {
        alpha = 0xff;
        red = (uint8_t) ((y / 768.0) * 255.0);
        green = (uint8_t) ((x / 1024.0) * 255.0);
        
        red = green = (red + green + blue) / 3;

        data[y * 1024 + x] = (alpha << 24) | (red << 16) | (green << 8) | blue;
      }
    }

  ply_video_buffer_fill_with_argb32_data (buffer, NULL, 0, 0, 1024, 768, 
                                          1024 * 4, data);
}

int
main (int    argc,
      char **argv)
{
  static unsigned int seed = 0;
  PlyVideoBuffer *buffer;
  int exit_code;

  exit_code = 0;

  buffer = ply_video_buffer_new (NULL);

  if (!ply_video_buffer_open (buffer))
    {
      exit_code = errno;
      perror ("could not open frame buffer");
      return exit_code;
    }

  if (seed == 0)
    {
      seed = (int) get_current_time ();
      srand (seed);
    }

  while ("we want to see ad-hoc animations")
    {
      animate_at_time (buffer, get_current_time ());
      usleep (1000000/30.);
    }

  ply_video_buffer_close (buffer);
  ply_video_buffer_free (buffer);

  return main (argc, argv);
}

#endif /* PLY_VIDEO_BUFFER_ENABLE_TEST */
