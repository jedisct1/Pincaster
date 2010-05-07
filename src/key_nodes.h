
#ifndef __KEY_NODES_H__
#define __KEY_NODES_H__ 1

int key_node_cmp(const KeyNode * const kn1, const KeyNode * const kn2);

int get_key_node_from_key(PanDB * const db, Key * const key,
                          const _Bool create,
                          KeyNode * * const key_node);

void free_key_node(KeyNode * const key_node);

#endif
