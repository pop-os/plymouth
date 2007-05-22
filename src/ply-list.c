#include "config.h"
#include "ply-list.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct _ply_list
{
  ply_list_node_t *first_node;
  ply_list_node_t *last_node;

  int number_of_nodes;
};

struct _ply_list_node
{
  void   *data;
  struct _ply_list_node *previous;
  struct _ply_list_node *next;
};

ply_list_t *
ply_list_new (void)
{
  ply_list_t *list;

  list = calloc (1, sizeof (ply_list_t));

  list->first_node = NULL;
  list->last_node = NULL;
  list->number_of_nodes = 0;

  return list;
}

void 
ply_list_free (ply_list_t *list)
{
  ply_list_node_t *node;

  if (list == NULL)
    return;

  node = list->first_node;
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      next_node = node->next;
      ply_list_remove_node (list, node);
      node = next_node;
    }

  free (list);
}

ply_list_node_t *
ply_list_node_new (void *data)
{
  ply_list_node_t *node;

  node = calloc (1, sizeof (ply_list_node_t));
  node->data = data;

  return node;
}

void 
ply_list_node_free (ply_list_node_t *node)
{
  if (node == NULL)
    return;

  assert ((node->previous == NULL) && (node->next == NULL));

  free (node);
}

int 
ply_list_get_length (ply_list_t *list)
{
  return list->number_of_nodes;
}

ply_list_node_t *
ply_list_find_node (ply_list_t *list,
                    void       *data)
{
  ply_list_node_t *node;

  node = list->first_node;
  while (node != NULL)
    {
      if (node->data == data)
        break;

      node = node->next;
    }
  return node;
}

static void
ply_list_insert_node (ply_list_t      *list,
                      ply_list_node_t *node_before,
                      ply_list_node_t *new_node)
{

  if (new_node == NULL)
    return;

  if (node_before == NULL)
    {
      if (list->first_node == NULL)
        { 
          assert (list->last_node == NULL);

          list->first_node = new_node;
          list->last_node = new_node;
        }
      else
        {
          list->first_node->previous = new_node;
          new_node->next = list->first_node;
          list->first_node = new_node;
        }
    }
  else
    {
      new_node->next = node_before->next;
      if (node_before->next != NULL)
        node_before->next->previous = new_node;
      node_before->next = new_node;
      new_node->previous = node_before;

      if (node_before == list->last_node)
        list->last_node = new_node;
    }

  list->number_of_nodes++;
}

ply_list_node_t *
ply_list_insert_data (ply_list_t      *list,
                      void            *data,
                      ply_list_node_t *node_before)
{
  ply_list_node_t *node;

  node = ply_list_node_new (data);

  ply_list_insert_node (list, node_before, node);

  return node;
}

ply_list_node_t *
ply_list_append_data (ply_list_t *list,
                      void       *data)
{
  return ply_list_insert_data (list, data, list->last_node);
}

ply_list_node_t *
ply_list_prepend_data (ply_list_t *list,
                       void       *data)
{
  return ply_list_insert_data (list, data, NULL);
}

void 
ply_list_remove_data (ply_list_t *list,
                      void       *data)
{
  ply_list_node_t *node;

  if (data == NULL)
    return;

  node = ply_list_find_node (list, data);

  if (node != NULL)
    ply_list_remove_node (list, node);
}

void 
ply_list_remove_node (ply_list_t      *list,
                      ply_list_node_t *node)
{
  if (node == NULL)
    return;

  if (node == list->first_node)
    list->first_node = node->next;

  if (node == list->last_node)
    list->last_node = node->previous;

  if (node->previous != NULL)
    {
      node->previous->next = node->next;
      node->previous = NULL;
    }

  if (node->next != NULL)
    {
      node->next->previous = node->previous;
      node->next = NULL;
    }

  ply_list_node_free (node);
  list->number_of_nodes--;
}

ply_list_node_t *
ply_list_get_first_node (ply_list_t *list)
{
  return list->first_node;
}

ply_list_node_t *
ply_list_get_next_node (ply_list_t     *list,
                       ply_list_node_t *node)
{
  return node->next;
}

void *
ply_list_node_get_data (ply_list_node_t *node)
{
  return node->data;
}

#ifdef PLY_LIST_ENABLE_TEST
#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  ply_list_t *list;
  ply_list_node_t *node;
  int i;

  list = ply_list_new ();

  ply_list_append_data (list, (void *) "foo");
  ply_list_append_data (list, (void *) "bar");
  ply_list_append_data (list, (void *) "baz");
  ply_list_prepend_data (list, (void *) "qux");
  ply_list_prepend_data (list, (void *) "quux");
  ply_list_remove_data (list, (void *) "baz");
  ply_list_remove_data (list, (void *) "foo");

  node = ply_list_get_first_node (list);
  i = 0;
  while (node != NULL)
    {
      printf ("node '%d' has data '%s'\n", i,
              (char *) ply_list_node_get_data (node));
      node = ply_list_get_next_node (list, node);
      i++;
    }

  ply_list_free (list);
  return 0;
}

#endif
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
