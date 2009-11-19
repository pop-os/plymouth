/* ply-image.c - png file loader
 *
 * Copyright (C) 2006, 2007 Red Hat, Inc.
 * Copyright (C) 2003 University of Southern California
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
 * Some implementation taken from the cairo library.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 *             Kristian Høgsberg <krh@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
 *             Carl D. Worth <cworth@cworth.org>
 */
#include "config.h"
#include "ply-image.h"
#include "ply-pixel-buffer.h"

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
#include <math.h>

#include <png.h>

#include <linux/fb.h>

#include "ply-utils.h"

struct _ply_image
{
  char  *filename;
  ply_pixel_buffer_t *buffer;
};

ply_image_t *
ply_image_new (const char *filename)
{
  ply_image_t *image;

  assert (filename != NULL);

  image = calloc (1, sizeof (ply_image_t));

  image->filename = strdup (filename);
  image->buffer = NULL;

  return image;
}

void
ply_image_free (ply_image_t *image)
{
  if (image == NULL)
    return;

  assert (image->filename != NULL);
  
  ply_pixel_buffer_free (image->buffer);
  free (image->filename);
  free (image);
}

static void
transform_to_argb32 (png_struct   *png,
                     png_row_info *row_info,
                     png_byte     *data)
{
  unsigned int i;

  for (i = 0; i < row_info->rowbytes; i += 4) 
  {
    uint8_t  red, green, blue, alpha;
    uint32_t pixel_value;

    red = data[i + 0];
    green = data[i + 1];
    blue = data[i + 2];
    alpha = data[i + 3];

    red = (uint8_t) CLAMP (((red / 255.0) * (alpha / 255.0)) * 255.0, 0, 255.0);
    green = (uint8_t) CLAMP (((green / 255.0) * (alpha / 255.0)) * 255.0,
                             0, 255.0);
    blue = (uint8_t) CLAMP (((blue / 255.0) * (alpha / 255.0)) * 255.0, 0, 255.0);

    pixel_value = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
    memcpy (data + i, &pixel_value, sizeof (uint32_t));
  }
}

bool
ply_image_load (ply_image_t *image)
{
  png_struct *png;
  png_info *info;
  png_uint_32 width, height, bytes_per_row, row;
  int bits_per_pixel, color_type, interlace_method;
  png_byte **rows;
  uint32_t *bytes;
  FILE *fp;
  
  assert (image != NULL);
  
  fp = fopen (image->filename, "r");
  if (fp == NULL)
    return false;
  
  png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  assert (png != NULL);

  info = png_create_info_struct (png);
  assert (info != NULL);

  png_init_io (png, fp);

  if (setjmp (png_jmpbuf (png)) != 0)
    {
      fclose (fp);
      return false;
    }

  png_read_info (png, info);
  png_get_IHDR (png, info,
                &width, &height, &bits_per_pixel,
                &color_type, &interlace_method, NULL, NULL);
  bytes_per_row = 4 * width;

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb (png);

  if ((color_type == PNG_COLOR_TYPE_GRAY) && (bits_per_pixel < 8))
    png_set_gray_1_2_4_to_8 (png);

  if (png_get_valid (png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha (png);

  if (bits_per_pixel == 16)
    png_set_strip_16 (png);

  if (bits_per_pixel < 8)
    png_set_packing (png);

  if ((color_type == PNG_COLOR_TYPE_GRAY)
      || (color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
    png_set_gray_to_rgb (png);

  if (interlace_method != PNG_INTERLACE_NONE)
    png_set_interlace_handling (png);

  png_set_filler (png, 0xff, PNG_FILLER_AFTER);

  png_set_read_user_transform_fn (png, transform_to_argb32);

  png_read_update_info (png, info);

  rows = malloc (height * sizeof (png_byte *));
  image->buffer = ply_pixel_buffer_new (width, height);
  
  bytes = ply_pixel_buffer_get_argb32_data (image->buffer);

  for (row = 0; row < height; row++)
    rows[row] = (png_byte*) &bytes[row * width];

  png_read_image (png, rows);

  free (rows);
  png_read_end (png, info);
  fclose (fp);
  png_destroy_read_struct (&png, &info, NULL);

  return true;
}

uint32_t *
ply_image_get_data (ply_image_t *image)
{
  assert (image != NULL);

  return ply_pixel_buffer_get_argb32_data (image->buffer);
}

long
ply_image_get_width (ply_image_t *image)
{
  ply_rectangle_t size;
  
  assert (image != NULL);
  ply_pixel_buffer_get_size (image->buffer, &size);

  return size.width;
}

long
ply_image_get_height (ply_image_t *image)
{
  ply_rectangle_t size;
  
  assert (image != NULL);
  ply_pixel_buffer_get_size (image->buffer, &size);

  return size.height;
}

static inline uint32_t
ply_image_interpolate (ply_image_t *image,
                       int          width,
                       int          height,
                       double       x,
                       double       y)
{
  int ix;
  int iy;
  
  int i;
  
  int offset_x;
  int offset_y;
  uint32_t pixels[2][2];
  uint32_t reply = 0;
  uint32_t *bytes;
  
  bytes = ply_pixel_buffer_get_argb32_data (image->buffer);
  
  for (offset_y = 0; offset_y < 2; offset_y++)
  for (offset_x = 0; offset_x < 2; offset_x++)
    {
      ix = x + offset_x;
      iy = y + offset_y;
      
      if (ix < 0 || ix >= width || iy < 0 || iy >= height)
        pixels[offset_y][offset_x] = 0x00000000;
      else
        pixels[offset_y][offset_x] = bytes[ix + iy * width];
    }
  if (!pixels[0][0] && !pixels[0][1] && !pixels[1][0] && !pixels[1][1]) return 0;
  
  ix = x;
  iy = y;
  x -= ix;
  y -= iy;
  for (i = 0; i < 4; i++)
    {
      uint32_t value = 0;
      uint32_t mask = 0xFF << (i * 8);
      value += ((pixels[0][0]) & mask) * (1-x) * (1-y);
      value += ((pixels[0][1]) & mask) * x * (1-y);
      value += ((pixels[1][0]) & mask) * (1-x) * y;
      value += ((pixels[1][1]) & mask) * x * y;
      reply |= value & mask;
    }
  return reply;
}

ply_image_t *
ply_image_resize (ply_image_t *image,
                  long         width,
                  long         height)
{
  ply_image_t *new_image;
  int x, y;
  double old_x, old_y;
  int old_width, old_height;
  float scale_x, scale_y;
  uint32_t *bytes;
  
  new_image = ply_image_new (image->filename);

  new_image->buffer = ply_pixel_buffer_new (width, height);

  bytes = ply_pixel_buffer_get_argb32_data (new_image->buffer);

  old_width = ply_image_get_width (image);
  old_height = ply_image_get_height (image);

  scale_x = ((double) old_width - 1) / MAX (width - 1, 1);
  scale_y = ((double) old_height - 1) / MAX (height - 1, 1);

  for (y = 0; y < height; y++)
    {
      old_y = y * scale_y;
      for (x=0; x < width; x++)
        {
          old_x = x * scale_x;
          bytes[x + y * width] =
                    ply_image_interpolate (image, old_width, old_height, old_x, old_y);
        }
    }
  return new_image;
}

ply_image_t *
ply_image_rotate (ply_image_t *image,
                  long         center_x,
                  long         center_y,
                  double       theta_offset)
{
  ply_image_t *new_image;
  int x, y;
  double old_x, old_y;
  int width;
  int height;
  uint32_t *bytes;

  width = ply_image_get_width (image);
  height = ply_image_get_height (image);

  new_image = ply_image_new (image->filename);

  new_image->buffer = ply_pixel_buffer_new (width, height);

  bytes = ply_pixel_buffer_get_argb32_data (new_image->buffer);

  double d = sqrt ((center_x * center_x +
                    center_y * center_y));
  double theta = atan2 (-center_y, -center_x) - theta_offset;
  double start_x = center_x + d * cos (theta);
  double start_y = center_y + d * sin (theta);
  double step_x = cos (-theta_offset);
  double step_y = sin (-theta_offset);
  
  for (y = 0; y < height; y++)
    {
      old_y = start_y;
      old_x = start_x;
      start_y += step_x;
      start_x -= step_y;
      for (x = 0; x < width; x++)
        {
          if (old_x < 0 || old_x > width || old_y < 0 || old_y > height)
            bytes[x + y * width] = 0;
          else
            bytes[x + y * width] =
                    ply_image_interpolate (image, width, height, old_x, old_y);
          old_x += step_x;
          old_y += step_y;
        }
    }
  return new_image;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
