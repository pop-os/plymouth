#ifndef PLY_LIST_H
#define PLY_LIST_H

typedef struct _ply_list_node ply_list_node_t;
typedef struct _ply_list ply_list_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_list_t *ply_list_new (void);
void ply_list_free (ply_list_t *list);
int ply_list_get_length (ply_list_t *list);
ply_list_node_t *ply_list_find_node (ply_list_t *list,
                                     void       *data);
ply_list_node_t *ply_list_insert_data (ply_list_t *list,
		                       void       *data,
		                       ply_list_node_t *node_before);
ply_list_node_t *ply_list_append_data (ply_list_t *list,
                                       void       *data);
ply_list_node_t *ply_list_prepend_data (ply_list_t *list,
                                        void       *data);
void ply_list_remove_data (ply_list_t *list,
                          void        *data);
void ply_list_remove_node (ply_list_t      *list,
                           ply_list_node_t *node);
ply_list_node_t *ply_list_get_first_node (ply_list_t *list);
ply_list_node_t *ply_list_get_next_node (ply_list_t  *list,
                                         ply_list_node_t *node);

void *ply_list_node_get_data (ply_list_node_t *node);
#endif

#endif /* PLY_LIST_H */
