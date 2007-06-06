/* ply-boot-splash-plugin.h - plugin interface for ply_boot_splash_t
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
#ifndef PLY_BOOT_SPLASH_PLUGIN_H
#define PLY_BOOT_SPLASH_PLUGIN_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-event-loop.h"

typedef struct _ply_boot_splash_plugin ply_boot_splash_plugin_t;

typedef struct 
{
  ply_boot_splash_plugin_t * (* create_plugin) (void);
  void (* destroy_plugin) (ply_boot_splash_plugin_t *plugin);

  bool (* show_splash_screen) (ply_boot_splash_plugin_t *plugin);
  void (* update_status) (ply_boot_splash_plugin_t *plugin,
                          const char               *status);
  void (* hide_splash_screen) (ply_boot_splash_plugin_t *plugin);
  void (* attach_to_event_loop) (ply_boot_splash_plugin_t *plugin,
                                 ply_event_loop_t         *loop);

} ply_boot_splash_plugin_interface_t;

#endif /* PLY_BOOT_SPLASH_PLUGIN_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
