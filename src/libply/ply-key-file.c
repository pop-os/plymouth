/* ply-key-file.c - key file loader
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
 * Written by: Ray Strode <rstrode@redhat.com>
 */
#include "config.h"
#include "ply-key-file.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ply-list.h"
#include "ply-utils.h"

typedef struct
{
  char *key;
  char *value;
} ply_key_file_entry_t;

typedef struct
{
  char *name;
  ply_list_t *entries;
} ply_key_file_group_t;

struct _ply_key_file
{
  char  *filename;
  FILE  *fp;

  ply_list_t *groups;
};

static bool ply_key_file_open_file (ply_key_file_t *key_file);
static void ply_key_file_close_file (ply_key_file_t *key_file);

static bool
ply_key_file_open_file (ply_key_file_t *key_file)
{
  assert (key_file != NULL);

  key_file->fp = fopen (key_file->filename, "r");

  if (key_file->fp == NULL)
    return false;
  return true;
}

static void
ply_key_file_close_file (ply_key_file_t *key_file)
{
  assert (key_file != NULL);

  if (key_file->fp == NULL)
    return;
  fclose (key_file->fp);
  key_file->fp = NULL;
}

ply_key_file_t *
ply_key_file_new (const char *filename)
{
  ply_key_file_t *key_file;

  assert (filename != NULL);

  key_file = calloc (1, sizeof (ply_key_file_t));

  key_file->filename = strdup (filename);
  key_file->fp = NULL;
  key_file->groups = ply_list_new ();

  return key_file;
}

void
ply_key_file_free (ply_key_file_t *key_file)
{
  if (key_file == NULL)
    return;

  assert (key_file->filename != NULL);

  ply_list_free (key_file->groups);
  free (key_file->filename);
  free (key_file);
}

static ply_key_file_group_t *
ply_key_file_load_group (ply_key_file_t *key_file,
                         const char     *group_name)
{
  int items_matched;
  ply_key_file_group_t *group;

  group = calloc (1, sizeof (ply_key_file_group_t));
  group->name = strdup (group_name);
  group->entries = ply_list_new ();

  do
    {
      ply_key_file_entry_t *entry;
      char *key;
      char *value;
      long offset;

      key = NULL;
      value = NULL;

      offset = ftell (key_file->fp);
      items_matched = fscanf (key_file->fp, " %a[^= \t\n] = %a[^\n] ", &key, &value);

      if (items_matched != 2)
        {
          if (items_matched == 1)
            fseek (key_file->fp, offset, SEEK_SET);

          free (key);
          free (value);
          break;
        }

      entry = calloc (1, sizeof (ply_key_file_entry_t));

      entry->key = key;
      entry->value = value;

      ply_list_append_data (group->entries, entry);
    }
  while (items_matched != EOF);

  return group;
}

static bool
ply_key_file_load_groups (ply_key_file_t *key_file)
{
  int items_matched;
  char *group_name;

  do
    {
      ply_key_file_group_t *group;

      items_matched = fscanf (key_file->fp, " [ %a[^]] ] ", &group_name);

      if (items_matched <= 0)
        break;

      group = ply_key_file_load_group (key_file, group_name);

      free (group_name);

      if (group == NULL)
        break;

      ply_list_append_data (key_file->groups, group);
    }
  while (items_matched != EOF);

  return ply_list_get_length (key_file->groups) > 0;
}

bool
ply_key_file_load (ply_key_file_t *key_file)
{
  bool was_loaded;

  assert (key_file != NULL);

  if (!ply_key_file_open_file (key_file))
    return false;

  was_loaded = ply_key_file_load_groups (key_file);

  ply_key_file_close_file (key_file);

  return was_loaded;
}

static ply_key_file_group_t *
ply_key_file_find_group (ply_key_file_t *key_file,
                         const char     *group_name)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (key_file->groups);

  while (node)
    {
      ply_key_file_group_t *group = ply_list_node_get_data (node);

      if (strcmp (group->name, group_name) == 0)
        return group;

      node = ply_list_get_next_node (key_file->groups, node);
    }

  return NULL;
}

static ply_key_file_entry_t *
ply_key_file_find_entry (ply_key_file_t       *key_file,
                         ply_key_file_group_t *group,
                         const char           *key)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (group->entries);

  while (node)
    {
      ply_key_file_entry_t *entry = ply_list_node_get_data (node);

      if (strcmp (entry->key, key) == 0)
        return entry;

      node = ply_list_get_next_node (group->entries, node);
    }

  return NULL;
}

bool
ply_key_file_has_key (ply_key_file_t *key_file,
                      const char     *group_name,
                      const char     *key)
{
  ply_key_file_group_t *group;
  ply_key_file_entry_t *entry;

  group = ply_key_file_find_group (key_file, group_name);

  if (group == NULL)
    return false;

  entry = ply_key_file_find_entry (key_file, group, key);

  return entry != NULL;
}

char *
ply_key_file_get_value (ply_key_file_t *key_file,
                        const char     *group_name,
                        const char     *key)
{
  ply_key_file_group_t *group;
  ply_key_file_entry_t *entry;

  group = ply_key_file_find_group (key_file, group_name);

  if (group == NULL)
    return NULL;

  entry = ply_key_file_find_entry (key_file, group, key);

  if (entry == NULL)
    return NULL;

  return strdup (entry->value);
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
