
#include "common.h"
#include "http_server.h"
#include "key_nodes.h"
#include "expirables.h"

RB_GENERATE(KeyNodes_, KeyNode_, entry, key_node_cmp);

int key_node_cmp(const KeyNode * const kn1, const KeyNode * const kn2)
{
    const Key * const k1 = kn1->key;
    const Key * const k2 = kn2->key;
    int ret;
    
    if (k1->len == k2->len) {
        ret = memcmp(k1->val, k2->val, k1->len);
    } else if (k1->len < k2->len) {
        if ((ret = memcmp(k1->val, k2->val, k1->len)) == 0) {
            ret = -1;
        }
    } else {
        if ((ret = memcmp(k1->val, k2->val, k2->len)) == 0) {
            ret = 1;
        }
    }
    return ret;
}

int get_key_node_from_key(PanDB * const db, Key * const key,
                          const _Bool create,
                          KeyNode * * const key_node)
{
    KeyNode *found_key_node;
    KeyNode scanned_key_node = { .key = key };
    KeyNode *new_key_node;

    *key_node = NULL;
    found_key_node = RB_FIND(KeyNodes_, &db->key_nodes, &scanned_key_node);
    if (found_key_node != NULL) {        
        *key_node = found_key_node;
        return 1;
    }
    if (create == 0) {
        return 0;
    }
    if ((new_key_node = malloc(sizeof *new_key_node)) == NULL) {
        return -1;
    }
    retain_key(key);
    *new_key_node = (KeyNode) {
        .key = key,
        .slot = NULL,
        .properties = NULL,
        .expirable = NULL
    };
    if (RB_INSERT(KeyNodes_, &db->key_nodes, new_key_node) != NULL) {
        release_key(key);
        free(new_key_node);
        return -1;
    }
    *key_node = new_key_node;
    
    return 2;
}

void free_key_node(PanDB * const db, KeyNode * const key_node)
{
    if (key_node == NULL) {
        return;
    }
    assert(key_node->slot == NULL);
    release_key(key_node->key);
    key_node->key = NULL;
    free_slip_map(&key_node->properties);
    key_node->properties = NULL;
    HttpHandlerContext * context = db->context;
    if (key_node->expirable != NULL) {
        remove_expirable_from_tree(db, key_node->expirable);        
        remove_entry_from_slab(&context->expirables_slab, key_node->expirable);
        key_node->expirable = NULL;
    }
    free(key_node);
}

SubSlots count_key_nodes(const KeyNodes * const key_nodes_)
{
    KeyNodes * const key_nodes = (KeyNodes *) key_nodes_;
    SubSlots count = (SubSlots) 0U;
    
    if (key_nodes == NULL) {
        return count;
    }
    KeyNode *key_node = NULL;
    RB_FOREACH(key_node, KeyNodes_, key_nodes) {
        count++;
    }
    return count;    
}

int key_nodes_foreach(KeyNodes * const key_nodes,
                      KeyNodesForeachCB cb, void * const context)
{
    if (key_nodes == NULL) {
        return 0;
    }
    int ret;
    KeyNode *key_node = NULL;
    RB_FOREACH(key_node, KeyNodes_, key_nodes) {
        if ((ret = cb(context, key_node)) != 0) {
            return ret;
        }
    }
    return 0;
}
