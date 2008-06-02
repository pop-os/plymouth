/* ply-frame-buffer.c - framebuffer abstraction
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
#include "ply-frame-buffer.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>

#include <linux/fb.h>

#ifndef PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME
#define PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME "/dev/fb"
#endif

struct _ply_frame_buffer
{
  char *device_name;
  int   device_fd;

  char *map_address;
  size_t size;

  uint32_t *shadow_buffer;

  uint32_t red_bit_position;
  uint32_t green_bit_position;
  uint32_t blue_bit_position;
  uint32_t alpha_bit_position;

  uint32_t bits_for_red;
  uint32_t bits_for_green;
  uint32_t bits_for_blue;
  uint32_t bits_for_alpha;

  unsigned int bytes_per_pixel;
  unsigned int row_stride;

  ply_frame_buffer_area_t area;
  ply_frame_buffer_area_t area_to_flush;

  void (*flush)(ply_frame_buffer_t *buffer);

  uint32_t is_paused : 1;
};

static bool ply_frame_buffer_open_device (ply_frame_buffer_t  *buffer);
static void ply_frame_buffer_close_device (ply_frame_buffer_t *buffer);
static bool ply_frame_buffer_query_device (ply_frame_buffer_t *buffer);
static bool ply_frame_buffer_map_to_device (ply_frame_buffer_t *buffer);
static uint_fast32_t inline ply_frame_buffer_pixel_value_to_device_pixel_value (
    ply_frame_buffer_t *buffer,
    uint32_t        pixel_value);

static void inline ply_frame_buffer_blend_value_at_pixel (ply_frame_buffer_t *buffer,
                                                   int             x,
                                                   int             y,
                                                   uint32_t        pixel_value);

static void ply_frame_buffer_fill_area_with_pixel_value (
    ply_frame_buffer_t     *buffer,
    ply_frame_buffer_area_t *area,
    uint32_t            pixel_value);

static void ply_frame_buffer_add_area_to_flush_area (ply_frame_buffer_t     *buffer,
                                                     ply_frame_buffer_area_t *area);

static bool ply_frame_buffer_flush (ply_frame_buffer_t *buffer);

static bool
ply_frame_buffer_open_device (ply_frame_buffer_t  *buffer)
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
ply_frame_buffer_close_device (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  if (buffer->map_address != MAP_FAILED)
    {
      munmap (buffer->map_address, buffer->size);
      buffer->map_address = MAP_FAILED;
    }

  if (buffer->device_fd >= 0)
    {
      close (buffer->device_fd);
      buffer->device_fd = -1;
    }
}

static void
flush_generic (ply_frame_buffer_t *buffer)
{
  unsigned long row, column;
  char *row_buffer;
  size_t bytes_per_row;
  unsigned long x1, y1, x2, y2;

  x1 = buffer->area_to_flush.x;
  y1 = buffer->area_to_flush.y;
  x2 = x1 + buffer->area_to_flush.width;
  y2 = y1 + buffer->area_to_flush.height;

  bytes_per_row = buffer->area_to_flush.width * buffer->bytes_per_pixel;
  row_buffer = malloc (buffer->row_stride * buffer->bytes_per_pixel);
  for (row = y1; row < y2; row++)
    {
      unsigned long offset;

      for (column = x1; column < x2; column++)
        {
          uint32_t pixel_value;
          uint_fast32_t device_pixel_value;

          pixel_value = buffer->shadow_buffer[row * buffer->row_stride + column];

          device_pixel_value =
            ply_frame_buffer_pixel_value_to_device_pixel_value (buffer,
                                                                pixel_value);

          memcpy (row_buffer + column * buffer->bytes_per_pixel,
                  &device_pixel_value, buffer->bytes_per_pixel);
        }

      offset = row * buffer->row_stride * buffer->bytes_per_pixel;
      memcpy (buffer->map_address + offset, row_buffer,
              buffer->area_to_flush.width * buffer->bytes_per_pixel);
    }
  free (row_buffer);
}

static void
flush_xrgb32 (ply_frame_buffer_t *buffer)
{
  unsigned long x1, y1, x2, y2, y;
  char *dst, *src;

  x1 = buffer->area_to_flush.x;
  y1 = buffer->area_to_flush.y;
  x2 = x1 + buffer->area_to_flush.width;
  y2 = y1 + buffer->area_to_flush.height;

  dst = &buffer->map_address[(y1 * buffer->row_stride + x1) * 4];
  src = (char *) &buffer->shadow_buffer[y1 * buffer->row_stride + x1];

  if (buffer->area_to_flush.width == buffer->row_stride)
    {
      memcpy (dst, src, buffer->area_to_flush.width * buffer->area_to_flush.height * 4);
      return;
    }

  for (y = y1; y < y2; y++)
    {
      memcpy (dst, src, buffer->area_to_flush.width * 4);
      dst += buffer->row_stride * 4;
      src += buffer->row_stride * 4;
    }
}

static bool 
ply_frame_buffer_query_device (ply_frame_buffer_t *buffer)
{
  struct fb_var_screeninfo variable_screen_info;
  struct fb_fix_screeninfo fixed_screen_info;

  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);

  if (ioctl (buffer->device_fd, FBIOGET_VSCREENINFO, &variable_screen_info) < 0)
    {
      return false;
    }

  buffer->area.x = variable_screen_info.xoffset;
  buffer->area.y = variable_screen_info.yoffset;
  buffer->area.width = variable_screen_info.xres;
  buffer->area.height = variable_screen_info.yres;

  buffer->red_bit_position = variable_screen_info.red.offset;
  buffer->bits_for_red = variable_screen_info.red.length;

  buffer->green_bit_position = variable_screen_info.green.offset;
  buffer->bits_for_green = variable_screen_info.green.length;

  buffer->blue_bit_position = variable_screen_info.blue.offset;
  buffer->bits_for_blue = variable_screen_info.blue.length;

  buffer->alpha_bit_position = variable_screen_info.transp.offset;
  buffer->bits_for_alpha = variable_screen_info.transp.length;

  if (ioctl(buffer->device_fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0) 
    {
      return false;
    }

  buffer->bytes_per_pixel = variable_screen_info.bits_per_pixel >> 3;
  buffer->row_stride = fixed_screen_info.line_length / buffer->bytes_per_pixel;
  buffer->size = buffer->area.height * buffer->row_stride * buffer->bytes_per_pixel;

  if (buffer->bytes_per_pixel == 4 &&
      buffer->red_bit_position == 16 &&
      buffer->green_bit_position == 8 &&
      buffer->blue_bit_position == 0)
    buffer->flush = flush_xrgb32;
  else
    buffer->flush = flush_generic;

  return true;
}

static bool
ply_frame_buffer_map_to_device (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);
  assert (buffer->size > 0);

  buffer->map_address = mmap (NULL, buffer->size, PROT_WRITE,
                              MAP_SHARED, buffer->device_fd, 0);

  return buffer->map_address != MAP_FAILED;
}

__attribute__((__pure__))
static inline uint_fast32_t 
ply_frame_buffer_pixel_value_to_device_pixel_value (ply_frame_buffer_t *buffer,
                                                    uint32_t        pixel_value)
{
  uint8_t r, g, b, a;

  a = pixel_value >> 24; 
  a >>= (8 - buffer->bits_for_alpha);

  r = (pixel_value >> 16) & 0xff; 
  r >>= (8 - buffer->bits_for_red);

  g = (pixel_value >> 8) & 0xff; 
  g >>= (8 - buffer->bits_for_green);

  b = pixel_value & 0xff; 
  b >>= (8 - buffer->bits_for_blue);

  return ((a << buffer->alpha_bit_position)
          | (r << buffer->red_bit_position)
          | (g << buffer->green_bit_position)
          | (b << buffer->blue_bit_position));
}

__attribute__((__pure__))
static inline uint32_t
blend_two_pixel_values (uint32_t pixel_value_1,
                        uint32_t pixel_value_2)
{
  uint8_t alpha_1, red_1, green_1, blue_1;
  uint8_t red_2, green_2, blue_2;
  uint_least16_t red, green, blue;
 
  assert (((uint8_t) (pixel_value_2 >> 24)) == 0xff);

  alpha_1 = (uint8_t) (pixel_value_1 >> 24);
  red_1 = (uint8_t) (pixel_value_1 >> 16);
  green_1 = (uint8_t) (pixel_value_1 >> 8);
  blue_1 = (uint8_t) pixel_value_1;

  red_2 = (uint8_t) (pixel_value_2 >> 16);
  green_2 = (uint8_t) (pixel_value_2 >> 8);
  blue_2 = (uint8_t) pixel_value_2;

  red = red_1 * 255 + red_2 * (255 - alpha_1); 
  green = green_1 * 255 + green_2 * (255 - alpha_1); 
  blue = blue_1 * 255 + blue_2 * (255 - alpha_1); 

  red = (uint8_t) ((red + (red >> 8) + 0x80) >> 8);
  green = (uint8_t) ((green + (green >> 8) + 0x80) >> 8);
  blue = (uint8_t) ((blue + (blue >> 8) + 0x80) >> 8);

  return 0xff000000 | (red << 16) | (green << 8) | blue;
}

__attribute__((__pure__))
static inline uint32_t
make_pixel_value_translucent (uint32_t pixel_value, 
                              uint8_t  opacity)
{
  uint_least16_t alpha, red, green, blue;

  if (opacity == 255)
    return pixel_value;

  alpha = (uint8_t) (pixel_value >> 24);
  red = (uint8_t) (pixel_value >> 16);
  green = (uint8_t) (pixel_value >> 8);
  blue = (uint8_t) pixel_value;

  red *= opacity;
  green *= opacity;
  blue *= opacity;
  alpha *= opacity;

  red = (uint8_t) ((red + (red >> 8) + 0x80) >> 8);
  green = (uint8_t) ((green + (green >> 8) + 0x80) >> 8);
  blue = (uint8_t) ((blue + (blue >> 8) + 0x80) >> 8);
  alpha = (uint8_t) ((alpha + (alpha >> 8) + 0x80) >> 8);

  return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

static inline void 
ply_frame_buffer_blend_value_at_pixel (ply_frame_buffer_t *buffer,
                                       int             x,
                                       int             y,
                                       uint32_t        pixel_value)
{
  uint32_t old_pixel_value;

  if ((pixel_value >> 24) != 0xff)
    {
      old_pixel_value = buffer->shadow_buffer[y * buffer->row_stride + x];

      pixel_value = blend_two_pixel_values (pixel_value, old_pixel_value);
    }

  buffer->shadow_buffer[y * buffer->row_stride + x] = pixel_value;
}

static void
ply_frame_buffer_fill_area_with_pixel_value (ply_frame_buffer_t     *buffer,
                                             ply_frame_buffer_area_t *area,
                                             uint32_t            pixel_value)
{
  unsigned long row, column;

  for (row = area->y; row < area->y + area->height; row++)
    {
      for (column = area->x; column < area->x + area->width; column++)
        {
          ply_frame_buffer_blend_value_at_pixel (buffer, 
                                                 column, row,
                                                 pixel_value);
        }
    }
}

static void
ply_frame_buffer_area_union (ply_frame_buffer_area_t *area1,
                             ply_frame_buffer_area_t *area2,
                             ply_frame_buffer_area_t *result)
{
  unsigned long x1, y1, x2, y2;

  if (area1->width == 0)
    {
      *result = *area2;
      return;
    }

  if (area2->width == 0)
    {
      *result = *area1;
      return;
    }

  x1 = area1->x + area1->width;
  y1 = area1->y + area1->height;
  x2 = area2->x + area2->width;
  y2 = area2->y + area2->height;

  result->x = MIN(area1->x, area2->x);
  result->y = MIN(area1->y, area2->y);
  result->width = MAX(x1, x2) - result->x;
  result->height = MAX(y1, y2) - result->y;
}

static void
ply_frame_buffer_add_area_to_flush_area (ply_frame_buffer_t      *buffer,
                                         ply_frame_buffer_area_t *area)
{
  assert (buffer != NULL);
  assert (area != NULL);
  assert (area->x >= buffer->area.x);
  assert (area->y >= buffer->area.y);
  assert (area->x + area->width <= buffer->area.x + buffer->area.width);
  assert (area->y + area->height <= buffer->area.y + buffer->area.height);
  assert (area->width >= 0);
  assert (area->height >= 0);

  ply_frame_buffer_area_union (&buffer->area_to_flush,
                               area,
                               &buffer->area_to_flush);
}

static bool
ply_frame_buffer_flush (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  if (buffer->is_paused)
    return true;

  (*buffer->flush) (buffer);

  buffer->area_to_flush.x = buffer->area.width - 1;
  buffer->area_to_flush.y = buffer->area.height - 1;
  buffer->area_to_flush.width = 0; 
  buffer->area_to_flush.height = 0; 

  return true;
}

ply_frame_buffer_t *
ply_frame_buffer_new (const char *device_name)
{
  ply_frame_buffer_t *buffer;

  buffer = calloc (1, sizeof (ply_frame_buffer_t));

  if (device_name != NULL)
    buffer->device_name = strdup (device_name);
  else if (getenv ("FRAMEBUFFER") != NULL)
    buffer->device_name = strdup (getenv ("FRAMEBUFFER"));
  else
    buffer->device_name = 
      strdup (PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME);

  buffer->map_address = MAP_FAILED;
  buffer->shadow_buffer = NULL;

  buffer->is_paused = false;

  return buffer;
}

void
ply_frame_buffer_free (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  if (ply_frame_buffer_device_is_open (buffer))
    ply_frame_buffer_close (buffer);

  free (buffer->device_name);
  free (buffer->shadow_buffer);
  free (buffer);
}

bool 
ply_frame_buffer_open (ply_frame_buffer_t *buffer)
{
  bool is_open;

  assert (buffer != NULL);

  is_open = false;

  if (!ply_frame_buffer_open_device (buffer))
    {
      goto out;
    }

  if (!ply_frame_buffer_query_device (buffer))
    {
      goto out;
    }

  if (!ply_frame_buffer_map_to_device (buffer))
    {
      goto out;
    }

  buffer->shadow_buffer =
    realloc (buffer->shadow_buffer, 4 * buffer->row_stride * buffer->area.height);
  memset (buffer->shadow_buffer, 0, 4 * buffer->row_stride * buffer->area.height);
  ply_frame_buffer_fill_with_color (buffer, NULL, 0.0, 0.0, 0.0, 1.0);

  is_open = true;

out:

  if (!is_open)
    {
      int saved_errno;

      saved_errno = errno;
      ply_frame_buffer_close_device (buffer);
      errno = saved_errno;
    }

  return is_open;
}

void
ply_frame_buffer_pause_updates (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  buffer->is_paused = true;
}

bool
ply_frame_buffer_unpause_updates (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  
  buffer->is_paused = false;
  return ply_frame_buffer_flush (buffer);
}

bool 
ply_frame_buffer_device_is_open (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  return buffer->device_fd >= 0 && buffer->map_address != MAP_FAILED;
}

char *
ply_frame_buffer_get_device_name (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));
  assert (buffer->device_name != NULL);

  return strdup (buffer->device_name);
}

void
ply_frame_buffer_set_device_name (ply_frame_buffer_t *buffer,
                                  const char     *device_name)
{
  assert (buffer != NULL);
  assert (!ply_frame_buffer_device_is_open (buffer));
  assert (device_name != NULL);
  assert (buffer->device_name != NULL);

  if (strcmp (buffer->device_name, device_name) != 0)
    {
      free (buffer->device_name);
      buffer->device_name = strdup (device_name);
    }
}

void 
ply_frame_buffer_close (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  assert (ply_frame_buffer_device_is_open (buffer));
  ply_frame_buffer_close_device (buffer);

  buffer->bytes_per_pixel = 0;
  buffer->area.x = 0;
  buffer->area.y = 0;
  buffer->area.width = 0;
  buffer->area.height = 0;
}

void 
ply_frame_buffer_get_size (ply_frame_buffer_t     *buffer,
                           ply_frame_buffer_area_t *size)
{
  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));
  assert (size != NULL);

  *size = buffer->area;
}

static void
ply_frame_buffer_area_intersect (ply_frame_buffer_area_t *area1,
                                 ply_frame_buffer_area_t *area2,
                                 ply_frame_buffer_area_t *result)
{
  unsigned long x1, y1, x2, y2;

  if (area1->width == 0)
    {
      *result = *area1;
      return;
    }

  if (area2->width == 0)
    {
      *result = *area2;
      return;
    }

  x1 = area1->x + area1->width;
  y1 = area1->y + area1->height;
  x2 = area2->x + area2->width;
  y2 = area2->y + area2->height;

  result->x = MAX(area1->x, area2->x);
  result->y = MAX(area1->y, area2->y);

  result->width = MIN(x1, x2) - result->x;
  result->height = MIN(y1, y2) - result->y;
}

bool
ply_frame_buffer_fill_with_gradient (ply_frame_buffer_t      *buffer,
                                     ply_frame_buffer_area_t *area,
                                     uint32_t                 start,
                                     uint32_t                 end)
{
/* The gradient produced is a linear interpolation of the two passed
 * in color stops: start and end.
 *
 * In order to prevent banding when the color stops are too close
 * together, or are stretched over too large an area, we slightly
 * perturb the intermediate colors as we generate them.
 *
 * Before we do this, we store the interpolated color values in a
 * fixed point number with lots of fractional bits.  This is so
 * we don't add noise after the values have been clamped to 8-bits
 *
 * We add random noise to all of the fractional bits of each color
 * channel and also NOISE_BITS worth of noise to the non-fractional
 * part of the color. By default NOISE_BITS is 1.
 *
 * We incorporate the noise by filling the bottom 24 bits of an
 * integer with random bits and then shifting the color channels
 * to the left such that the top 8 bits of the channel overlap
 * the noise by NOISE_BITS. E.g., if NOISE_BITS is 1, then the top
 * 7 bits of each channel won't overlap with the noise, and the 8th
 * bit + fractional bits will.  When the noise and color channel
 * are properly aligned, we add them together, drop the precision
 * of the resulting channels back to 8 bits and stuff the results
 * into a pixel in the frame buffer.
 */
#define NOISE_BITS 1
/* In the color stops, red is 8 bits starting at position 24
 * (since they're argb32 pixels).
 * We want to move those 8 bits such that the bottom NOISE_BITS
 * of them overlap the top of the 24 bits of generated noise.
 * Of course, green and blue are 8 bits away from red and each
 * other, respectively.
 */
#define RED_SHIFT (32 - (24 + NOISE_BITS))
#define GREEN_SHIFT (RED_SHIFT + 8)
#define BLUE_SHIFT (GREEN_SHIFT + 8)
#define NOISE_MASK (0x00ffffff)

/* Once, we've lined up the color channel we're interested in with
 * the noise, we need to mask out the other channels.
 */
#define COLOR_MASK (0xff << (24 - NOISE_BITS))

  uint32_t red, green, blue, red_step, green_step, blue_step, t, pixel;
  uint32_t x, y;
  ply_frame_buffer_area_t cropped_area;

  if (area == NULL)
    area = &buffer->area;

  ply_frame_buffer_area_intersect (area, &buffer->area, &cropped_area);

  red   = (start << RED_SHIFT) & COLOR_MASK;
  green = (start << GREEN_SHIFT) & COLOR_MASK;
  blue  = (start << BLUE_SHIFT) & COLOR_MASK;

  t = (end << RED_SHIFT) & COLOR_MASK;
  red_step = (int32_t) (t - red) / (int32_t) buffer->area.height;
  t = (end << GREEN_SHIFT) & COLOR_MASK;
  green_step = (int32_t) (t - green) / (int32_t) buffer->area.height;
  t = (end << BLUE_SHIFT) & COLOR_MASK;
  blue_step = (int32_t) (t - blue) / (int32_t) buffer->area.height;

  /* we use a fixed seed so that the dithering doesn't change on repaints
   * of the same area.
   */
  srand(100200);

/* FIXME: we assume RAND_MAX is at least 24 bits here, and it is on linux.
 * On some platforms it's only 16its though.  If that were true on linux,
 * then NOISE_BITS would get effectively ignored, since those bits would
 * always overlap with zeros.  We could fix it by running rand() twice
 * per channel generating 32-bits of noise, or by shifting the result of
 * rand() over 8 bits, such that the zeros would be overlapping with the
 * least significant fractional bits of the color channel instead.
 */
#define NOISE() (rand () & NOISE_MASK)

  for (y = buffer->area.y; y < buffer->area.y + buffer->area.height; y++)
    {
      if (cropped_area.y <= y && y < cropped_area.y + cropped_area.height)
        {
          for (x = cropped_area.x; x < cropped_area.x + cropped_area.width; x++)
            {
              pixel =
                  0xff000000 |
                  (((red   + NOISE ()) & COLOR_MASK) >> RED_SHIFT) |
                  (((green + NOISE ()) & COLOR_MASK) >> GREEN_SHIFT) |
                  (((blue  + NOISE ()) & COLOR_MASK) >> BLUE_SHIFT);

              buffer->shadow_buffer[y * buffer->row_stride + x] = pixel;
            }
        }

      red += red_step;
      green += green_step;
      blue += blue_step;
    }

  ply_frame_buffer_add_area_to_flush_area (buffer, &cropped_area);

  return ply_frame_buffer_flush (buffer);
}

bool 
ply_frame_buffer_fill_with_color (ply_frame_buffer_t      *buffer,
                                  ply_frame_buffer_area_t  *area,
                                  double               red, 
                                  double               green,
                                  double               blue, 
                                  double               alpha)
{
  uint32_t pixel_value;
  ply_frame_buffer_area_t cropped_area;

  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));

  if (area == NULL)
    area = &buffer->area;

  ply_frame_buffer_area_intersect (area, &buffer->area, &cropped_area);

  red *= alpha;
  green *= alpha;
  blue *= alpha;

  pixel_value = PLY_FRAME_BUFFER_COLOR_TO_PIXEL_VALUE (red, green, blue, alpha);

  ply_frame_buffer_fill_area_with_pixel_value (buffer, &cropped_area, pixel_value);

  ply_frame_buffer_add_area_to_flush_area (buffer, &cropped_area);

  return ply_frame_buffer_flush (buffer);
}

bool
ply_frame_buffer_fill_with_hex_color_at_opacity (ply_frame_buffer_t      *buffer,
                                                 ply_frame_buffer_area_t *area,
                                                 uint32_t                 hex_color,
                                                 double                   opacity)
{
  ply_frame_buffer_area_t cropped_area;
  uint32_t pixel_value;
  double red;
  double green;
  double blue;
  double alpha;

  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));

  if (area == NULL)
    area = &buffer->area;

  ply_frame_buffer_area_intersect (area, &buffer->area, &cropped_area);

  /* if they only gave an rgb hex number, assume an alpha of 0xff
   */
  if ((hex_color & 0xff000000) == 0)
    hex_color = (hex_color << 8) | 0xff;

  red = ((double) (hex_color & 0xff000000) / 0xff000000);
  green = ((double) (hex_color & 0x00ff0000) / 0x00ff0000);
  blue = ((double) (hex_color & 0x0000ff00) / 0x0000ff00);
  alpha = ((double) (hex_color & 0x000000ff) / 0x000000ff);

  alpha *= opacity;

  red *= alpha;
  green *= alpha;
  blue *= alpha;

  pixel_value = PLY_FRAME_BUFFER_COLOR_TO_PIXEL_VALUE (red, green, blue, alpha);

  ply_frame_buffer_fill_area_with_pixel_value (buffer, &cropped_area, pixel_value);

  ply_frame_buffer_add_area_to_flush_area (buffer, &cropped_area);

  return ply_frame_buffer_flush (buffer);
}

bool
ply_frame_buffer_fill_with_hex_color (ply_frame_buffer_t      *buffer,
                                      ply_frame_buffer_area_t *area,
                                      uint32_t                 hex_color)
{
  return ply_frame_buffer_fill_with_hex_color_at_opacity (buffer, area, hex_color, 1.0);
}

bool 
ply_frame_buffer_fill_with_argb32_data_at_opacity (ply_frame_buffer_t      *buffer,
                                                   ply_frame_buffer_area_t *area,
                                                   unsigned long            x,
                                                   unsigned long            y,
                                                   uint32_t                *data,
                                                   double                   opacity)
{
  long row, column;
  uint8_t opacity_as_byte;
  ply_frame_buffer_area_t cropped_area;

  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));

  if (area == NULL)
    area = &buffer->area;

  ply_frame_buffer_area_intersect (area, &buffer->area, &cropped_area);

  opacity_as_byte = (uint8_t) (opacity * 255.0);

  for (row = y; row < y + cropped_area.height; row++)
    {
      for (column = x; column < x + cropped_area.width; column++)
        {
          uint32_t pixel_value;

          pixel_value = data[area->width * row + column];
          pixel_value = make_pixel_value_translucent (pixel_value, opacity_as_byte);
          ply_frame_buffer_blend_value_at_pixel (buffer,
                                                 cropped_area.x + (column - x),
                                                 cropped_area.y + (row - y),
                                                 pixel_value);

        }
    }

  ply_frame_buffer_add_area_to_flush_area (buffer, &cropped_area);

  return ply_frame_buffer_flush (buffer);
}

bool 
ply_frame_buffer_fill_with_argb32_data (ply_frame_buffer_t     *buffer,
                                        ply_frame_buffer_area_t *area,
                                        unsigned long       x,
                                        unsigned long       y,
                                        uint32_t           *data)
{
  return ply_frame_buffer_fill_with_argb32_data_at_opacity (buffer, area,
                                                            x, y, data, 1.0);
}

#ifdef PLY_FRAME_BUFFER_ENABLE_TEST

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
animate_at_time (ply_frame_buffer_t *buffer,
                 double          time)
{
  int x, y;
  uint32_t *data;
  ply_frame_buffer_area_t area;

  ply_frame_buffer_get_size (buffer, &area);

  data = calloc (area.width * area.height, sizeof (uint32_t));

  for (y = 0; y < area.height; y++)
    {
      int blue_bit_position;
      uint8_t red, green, blue, alpha;

      blue_bit_position = (int) 64 * (.5 * sin (time) + .5) + (255 - 64);
      blue = rand () % blue_bit_position;
      for (x = 0; x < area.width; x++)
      {
        alpha = 0xff;
        red = (uint8_t) ((y / (area.height * 1.0)) * 255.0);
        green = (uint8_t) ((x / (area.width * 1.0)) * 255.0);

        red = green = (red + green + blue) / 3;

        data[y * area.width + x] = (alpha << 24) | (red << 16) | (green << 8) | blue;
      }
    }

  ply_frame_buffer_fill_with_argb32_data (buffer, NULL, 0, 0, data);
}

int
main (int    argc,
      char **argv)
{
  static unsigned int seed = 0;
  ply_frame_buffer_t *buffer;
  int exit_code;

  exit_code = 0;

  buffer = ply_frame_buffer_new (NULL);

  if (!ply_frame_buffer_open (buffer))
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

  ply_frame_buffer_close (buffer);
  ply_frame_buffer_free (buffer);

  return main (argc, argv);
}

#endif /* PLY_FRAME_BUFFER_ENABLE_TEST */

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
