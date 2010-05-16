
#include "common.h"
#include "http_server.h"
#include "expirables.h"

static int expirable_cmp(const Expirable * const exp1,
                         const Expirable * const exp2);

RB_PROTOTYPE_STATIC(Expirables_, Expirable_, entry, expirable_cmp);
RB_GENERATE_STATIC(Expirables_, Expirable_, entry, expirable_cmp);

RB_PROTOTYPE_STATIC(KeyNodes_, KeyNode_, entry, key_node_cmp);
RB_GENERATE_STATIC(KeyNodes_, KeyNode_, entry, key_node_cmp);

static int expirable_cmp(const Expirable * const exp1,
                         const Expirable * const exp2)
{
    if (exp1->ts < exp2->ts) {
        return -1;
    }
    if (exp1->ts > exp2->ts) {
        return 1;
    }
    if (exp1->key_node < exp2->key_node) {
        return -1;
    } else {
        return 1;
    }
    assert(0);
    
    return 0;
}

int add_expirable_to_tree(PanDB * const pan_db, Expirable * const expirable)
{
    assert(RB_INSERT(Expirables_, &pan_db->expirables, expirable) == NULL);
    
    return 0;
}

int remove_expirable_from_tree(PanDB * const pan_db,
                               Expirable * const expirable)
{
    if (RB_REMOVE(Expirables_, &pan_db->expirables, expirable) == NULL) {
        return -1;
    }    
    return 0;
}

typedef struct HasExpiredKeysInLayerCBContext_ {
    HttpHandlerContext * const context;
    _Bool has_expired_keys;
} HasExpiredKeysInLayerCBContext;

static int has_expired_keys_in_layer(void *cb_context_, void *entry,
                                     const size_t sizeof_entry)
{
    (void) sizeof_entry;
    HasExpiredKeysInLayerCBContext *cb_context = cb_context_;
    HttpHandlerContext * const context = cb_context->context;
    const time_t now = context->now;    
    Layer * const layer = entry;    
    PanDB * const pan_db = &layer->pan_db;
    Expirables * const expirables = &pan_db->expirables;
    if (expirables == NULL) {
        return 0;
    }
    const Expirable * const expirable = RB_MIN(Expirables_, expirables);
    if (expirable != NULL && now >= expirable->ts) {
        cb_context->has_expired_keys = 1;
        return 1;
    }
    return 0;
}
    
static _Bool has_expired_keys(HttpHandlerContext * const context)
{
    HasExpiredKeysInLayerCBContext cb_context = {
        .context = context,
        .has_expired_keys = 0
    };    
    pthread_rwlock_rdlock(&context->rwlock_layers);
    slab_foreach(&context->layers_slab, has_expired_keys_in_layer,
                 &cb_context);
    pthread_rwlock_unlock(&context->rwlock_layers);
    
    return cb_context.has_expired_keys;
}

typedef struct PurgeExpiredKeysFromLayerCBContext_ {
    HttpHandlerContext * const context;
    _Bool did_purge;
} PurgeExpiredKeysFromLayerCBContext;

static int purge_expired_keys_from_layer(void *cb_context_, void *entry,
                                         const size_t sizeof_entry)
{
    (void) sizeof_entry;
    PurgeExpiredKeysFromLayerCBContext *cb_context = cb_context_;
    HttpHandlerContext * const context = cb_context->context;
    const time_t now = context->now;
    Layer * const layer = entry;    
    PanDB * const pan_db = &layer->pan_db;
    Expirables * const expirables = &pan_db->expirables;
    Expirable *expirable;
    Expirable *next;    
    KeyNode *key_node;

    if (expirables == NULL) {
        return 0;
    }
    for (expirable = RB_MIN(Expirables_, expirables); expirable != NULL;
         expirable = next) {
        if (now < expirable->ts) {
            break;
        }
        next = RB_NEXT(Expirables_, expirables, expirable);
        key_node = expirable->key_node;
        assert(key_node != NULL);
        assert(key_node->expirable != NULL);        
        assert(key_node->expirable == expirable);
        if (key_node->slot != NULL) {
            if (remove_entry_from_key_node(pan_db, key_node, 0) != 0) {
                return -1;
            }
            key_node->slot = NULL;
        }
        assert(key_node->slot == NULL);
        RB_REMOVE(KeyNodes_, &pan_db->key_nodes, key_node);
        free_key_node(pan_db, key_node);
        cb_context->did_purge = 1;
    }
    return 0;
}

int purge_expired_keys(HttpHandlerContext * const context)
{
    if (has_expired_keys(context) == 0) {
        return 0;
    }
    PurgeExpiredKeysFromLayerCBContext cb_context = {
        .context = context,
        .did_purge = 0
    };
    pthread_rwlock_wrlock(&context->rwlock_layers);
    slab_foreach(&context->layers_slab, purge_expired_keys_from_layer,
                 &cb_context);
    pthread_rwlock_unlock(&context->rwlock_layers);
    
    return (int) cb_context.did_purge;
}
