
#include "common.h"
#include "http_server.h"
#include "domain_layers.h"

int handle_domain_layers(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req)
{
    Key *layer_name;
    
    (void) opts;
    if (req->type == EVHTTP_REQ_GET) {
        Op op;
        LayersIndexOp * const index_op = &op.layers_index_op;
       
        if (strcmp(uri, "index") != 0) {
            return HTTP_NOTFOUND;
        }
        *index_op = (LayersIndexOp) {
            .type = OP_TYPE_LAYERS_INDEX,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid
        };
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, index_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        
        return 0;
    }
    if (req->type == EVHTTP_REQ_POST) {
        Op op;
        LayersCreateOp * const create_op = &op.layers_create_op;
       
        if (*uri == 0) {
            return HTTP_NOTFOUND;
        }
        if ((layer_name = new_key_from_c_string(uri)) == NULL) {
            return HTTP_SERVUNAVAIL;
        }
        *create_op = (LayersCreateOp) {
            .type = OP_TYPE_LAYERS_CREATE,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name
        };
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, create_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        *write_to_log = 1;
        
        return 0;
    }
    if (req->type == EVHTTP_REQ_DELETE) {
        Op op;
        LayersDeleteOp * const delete_op = &op.layers_delete_op;
       
        if (*uri == 0) {
            return HTTP_NOTFOUND;
        }
        if ((layer_name = new_key_from_c_string(uri)) == NULL) {
            return HTTP_SERVUNAVAIL;
        }
        *delete_op = (LayersDeleteOp) {
            .type = OP_TYPE_LAYERS_DELETE,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name
        };
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, delete_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        *write_to_log = 1;
        
        return 0;
    }
    return HTTP_NOTFOUND;
}

typedef struct GetLayerByLayerNameCBContext_ {
    const char *layer_name;    
    Layer *layer;
} GetLayerByLayerNameCBContext;

int get_layer_by_layer_name_cb(void *context_, void *entry,
                               const size_t sizeof_entry)
{
    (void) sizeof_entry;
    GetLayerByLayerNameCBContext *context = context_;
    Layer * const layer = entry;
    
    if (strcmp(context->layer_name, layer->name) != 0) {
        return 0;
    }
    context->layer = layer;
    
    return 1;
}

static Layer *register_new_layer(HttpHandlerContext * const context_,
                                 const char * const layer_name)
{
    HttpHandlerContext * const context = context_;    
    Layer tmp_layer;
    Layer *layer;
    
    if (*layer_name == 0) {
        return NULL;
    }
    if ((tmp_layer.name = strdup(layer_name)) == NULL) {
        return NULL;
    }
    layer = add_entry_to_slab(&context->layers_slab, &tmp_layer);
    if (layer == NULL) {
        free(tmp_layer.name);
        return NULL;
    }
    if (init_pan_db(&layer->pan_db, context) != 0) {
        assert(tmp_layer.name == layer->name);
        free(layer->name);
        remove_entry_from_slab(&context->layers_slab, layer);
        layer = NULL;
    }
    return layer;
}

int get_pan_db_by_layer_name(HttpHandlerContext * const context_,
                             const char * const layer_name,
                             const _Bool create,
                             PanDB * * const pan_db)
{
    HttpHandlerContext * const context = context_;
    GetLayerByLayerNameCBContext cb_context = {
        .layer_name = layer_name,
        .layer = NULL
    };
    *pan_db = NULL;
    slab_foreach(&context->layers_slab, get_layer_by_layer_name_cb,
                 &cb_context);
    if (cb_context.layer != NULL) {
        *pan_db = &cb_context.layer->pan_db;
        return 0;
    }
    if (create == 0) {
        return -1;
    }
    Layer * const layer = register_new_layer(context, layer_name);
    if (layer == NULL) {
        return -2;
    }
    *pan_db = &layer->pan_db;
    
    return 1;
}

int handle_op_layers_create(LayersCreateOp * const create_op,
                            HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;
    int status;
    
    status = get_pan_db_by_layer_name
        (context, create_op->layer_name->val, 1, &pan_db);
    release_key(create_op->layer_name);
    if (pan_db == NULL) {
        assert(status < 0);
    }
    if (create_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    LayersCreateOpReply * const create_op_reply =
        &op_reply->layers_create_op_reply;
    
    *create_op_reply = (LayersCreateOpReply) {
        .type = OP_TYPE_LAYERS_CREATE,
        .req = create_op->req,
        .op_tid = create_op->op_tid,
        .json_gen = NULL                        
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    create_op_reply->json_gen = json_gen;
    const char *text_status;    
    yajl_gen_string(json_gen, (const unsigned char *) "status",
                    (unsigned int) sizeof "status" - (size_t) 1U);
    if (status == 0) {
        text_status = "existing";
    } else if (status > 0) {
        text_status = "created";
    } else {
        text_status = "error";
    }
    yajl_gen_string(json_gen, (const unsigned char *) text_status,
                    (unsigned int) strlen(text_status));
    send_op_reply(context, op_reply);
    
    return 0;
}

static int delete_layer_by_layer_name(HttpHandlerContext * const context_,
                                      const char * const layer_name)
{
    HttpHandlerContext * const context = context_;
    GetLayerByLayerNameCBContext cb_context = {
        .layer_name = layer_name,
        .layer = NULL
    };
    Layer *layer = NULL;
    slab_foreach(&context->layers_slab, get_layer_by_layer_name_cb,
                 &cb_context);
    if ((layer = cb_context.layer) == NULL) {
        return 1;
    }
    free_pan_db(&layer->pan_db);
    free(layer->name);
    layer->name = NULL;    
    if (remove_entry_from_slab(&context->layers_slab, layer) != 0) {
        return -1;
    }
    return 0;
}

int handle_op_layers_delete(LayersDeleteOp * const delete_op,
                            HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    int status;
    
    status = delete_layer_by_layer_name(context, delete_op->layer_name->val);
    release_key(delete_op->layer_name);
    if (status > 0) {
        return HTTP_NOTFOUND;
    } else if (status < 0) {
        return HTTP_SERVUNAVAIL;
    }
    if (delete_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }                
    LayersDeleteOpReply * const delete_op_reply =
        &op_reply->layers_delete_op_reply;
    
    *delete_op_reply = (LayersDeleteOpReply) {
        .type = OP_TYPE_LAYERS_DELETE,
        .req = delete_op->req,
        .op_tid = delete_op->op_tid,
        .json_gen = NULL
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    delete_op_reply->json_gen = json_gen;
    yajl_gen_string(json_gen, (const unsigned char *) "status",
                    (unsigned int) sizeof "status" - (size_t) 1U);
    yajl_gen_string(json_gen, (const unsigned char *) "deleted",
                    (unsigned int) sizeof "deleted" - (size_t) 1U);
    send_op_reply(context, op_reply);
    
    return 0;
}

typedef struct AddLayerNameToJsonCBContext_ {
    yajl_gen json_gen;
} AddLayerNameToJsonCBContext;

int add_layer_name_to_json_gen(void *context_, void *entry,
                               const size_t sizeof_entry)
{
    (void) sizeof_entry;
    AddLayerNameToJsonCBContext *context = context_;
    yajl_gen json_gen = context->json_gen;    
    Layer * const layer = entry;
    const PanDB * const pan_db = &layer->pan_db;
    
    yajl_gen_map_open(json_gen);
    yajl_gen_string(json_gen,
                    (const unsigned char *) "name",
                    (unsigned int) sizeof "name" - (size_t) 1U);
    yajl_gen_string(json_gen,
                    (const unsigned char *) layer->name,
                    (unsigned int) strlen(layer->name));
    
    const SubSlots nb_key_nodes = count_key_nodes(&pan_db->key_nodes);
    
    yajl_gen_string(json_gen,
                    (const unsigned char *) "records",
                    (unsigned int) sizeof "records" - (size_t) 1U);
    yajl_gen_integer(json_gen, (long) nb_key_nodes);

    yajl_gen_string(json_gen,
                    (const unsigned char *) "geo_records",
                    (unsigned int) sizeof "geo_records" - (size_t) 1U);
    yajl_gen_integer(json_gen, (long) pan_db->root.sub_slots);

    assert(nb_key_nodes >= pan_db->root.sub_slots);
    
    yajl_gen_string(json_gen,
                    (const unsigned char *) "type",
                    (unsigned int) sizeof "type" - (size_t) 1U);
    const char *type;
    switch (pan_db->layer_type) {
    case LAYER_TYPE_FLAT:
        type = "flat"; break;
    case LAYER_TYPE_FLATWRAP:
        type = "flatwrap"; break;
    case LAYER_TYPE_SPHERICAL:
        type = "spherical"; break;
    case LAYER_TYPE_GEOIDAL:
        type = "geoidal"; break;
    default:
        type = "unknown";
    }
    yajl_gen_string(json_gen,
                    (const unsigned char *) type,
                    (unsigned int) strlen(type));

    yajl_gen_string(json_gen,
                    (const unsigned char *) "distance_accuracy",
                    (unsigned int) sizeof "distance_accuracy" - (size_t) 1U);
    const char *accuracy;
    if (pan_db->layer_type == LAYER_TYPE_SPHERICAL ||
        pan_db->layer_type == LAYER_TYPE_GEOIDAL) {
        switch (pan_db->accuracy) {
        case ACCURACY_HS:
            accuracy = "haversine"; break;        
        case ACCURACY_GC:
            accuracy = "great_circle"; break;        
        case ACCURACY_FAST:
            accuracy = "fast"; break;
        case ACCURACY_ROMBOID:
            accuracy = "romboid"; break;
        default:
            accuracy = "unknown";
        }
    } else {
        accuracy = "euclidean";
    }
    yajl_gen_string(json_gen,
                    (const unsigned char *) accuracy,
                    (unsigned int) strlen(accuracy));
    
    yajl_gen_string(json_gen,
                    (const unsigned char *) "latitude_accuracy",
                    (unsigned int) sizeof "latitude_accuracy" - (size_t) 1U);
    yajl_gen_double(json_gen, (double) pan_db->latitude_accuracy);
    yajl_gen_string(json_gen,
                    (const unsigned char *) "longitude_accuracy",
                    (unsigned int) sizeof "longitude_accuracy" - (size_t) 1U);
    yajl_gen_double(json_gen, (double) pan_db->latitude_accuracy);
    
    yajl_gen_string(json_gen,
                    (const unsigned char *) "bounds",
                    (unsigned int) sizeof "bounds" - (size_t) 1U);
    yajl_gen_array_open(json_gen);
    yajl_gen_double(json_gen, (double) pan_db->qbounds.edge0.latitude);
    yajl_gen_double(json_gen, (double) pan_db->qbounds.edge0.longitude);
    yajl_gen_double(json_gen, (double) pan_db->qbounds.edge1.latitude);
    yajl_gen_double(json_gen, (double) pan_db->qbounds.edge1.longitude);    
    yajl_gen_array_close(json_gen);
    
    yajl_gen_map_close(json_gen);    
    return 0;
}

int handle_op_layers_index(LayersIndexOp * const index_op,
                            HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    
    if (index_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    LayersIndexOpReply * const index_op_reply =
        &op_reply->layers_index_op_reply;
    
    *index_op_reply = (LayersIndexOpReply) {
        .type = OP_TYPE_LAYERS_INDEX,
        .req = index_op->req,
        .op_tid = index_op->op_tid,
        .json_gen = NULL
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    index_op_reply->json_gen = json_gen;
    yajl_gen_string(json_gen, (const unsigned char *) "layers",
                    (unsigned int) sizeof "layers" - (size_t) 1U);
    yajl_gen_array_open(json_gen);
    
    AddLayerNameToJsonCBContext cb_context = {
        .json_gen = json_gen
    };
    slab_foreach(&context->layers_slab, add_layer_name_to_json_gen,
                 &cb_context);
    yajl_gen_array_close(json_gen);
    send_op_reply(context, op_reply);
    
    return 0;
}
