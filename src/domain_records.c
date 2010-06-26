
#include "common.h"
#include "http_server.h"
#include "domain_records.h"
#include "domain_layers.h"
#include "query_parser.h"
#include "expirables.h"

#ifndef PROPERTIES_DEFAULT_SLIP_MAP_BUFFER_SIZE
# define PROPERTIES_DEFAULT_SLIP_MAP_BUFFER_SIZE (size_t) 32U
#endif

typedef struct RecordsPutOptParseCBContext_ {
    RecordsPutOp *put_op;
} RecordsPutOptParseCBContext;

static int records_put_opt_parse_cb(void * const context_,
                                    const BinVal *key, const BinVal *value)
{
    RecordsPutOptParseCBContext * const context = context_;
    RecordsPutOp * const put_op = context->put_op;

    if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, INT_PROPERTY_POSITION)) {
        char *sep;
        char *svalue = value->val;
        char *zeroed1;

        skip_spaces((const char * *) &svalue);
        if (*svalue == 0 || (sep = strchr(svalue, ',')) == NULL) {
            return -1;
        }
        zeroed1 = sep;
        *sep++ = 0;
        skip_spaces((const char * *) &sep);
        if (*sep == 0) {
            return -1;
        }
        char *endptr;
        put_op->position.latitude = (Dimension) strtod(svalue, &endptr);
        if (endptr == NULL || endptr == svalue) {
            return -1;
        }
        put_op->position.longitude = (Dimension) strtod(sep, &endptr);
        if (endptr == NULL || endptr == sep) {
            return -1;
        }
        *zeroed1 = ',';
        put_op->position_set = 1;
    } else if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, INT_PROPERTY_EXPIRES_AT)) {
        char *svalue = value->val;
        skip_spaces((const char * *) &svalue);
        if (*svalue == 0) {
            put_op->expires_at = (time_t) 0;
            return 0;
        }
        char *endptr;
        put_op->expires_at = (time_t) strtoul(svalue, &endptr, 10);
        if (endptr == NULL || endptr == svalue) {
            return -1;
        }
    } else {
        assert(key->size > (size_t) 0U);
        if (*key->val == INT_PROPERTY_COMMON_PREFIX) {
            if (put_op->special_properties == NULL) {
                put_op->special_properties = new_slip_map
                    (PROPERTIES_DEFAULT_SLIP_MAP_BUFFER_SIZE);
                if (put_op->special_properties == NULL) {
                    return -1;
                }
            }
            replace_entry_in_slip_map(&put_op->special_properties,
                                      key->val, key->size,
                                      value->val, value->size);            
        } else {
            if (put_op->properties == NULL) {
                put_op->properties = new_slip_map
                    (PROPERTIES_DEFAULT_SLIP_MAP_BUFFER_SIZE);
                if (put_op->properties == NULL) {
                    return -1;
                }
            }
            replace_entry_in_slip_map(&put_op->properties,
                                      key->val, key->size,
                                      value->val, value->size);
        }
    }
    return 0;
}

typedef struct RecordsOptParseCBContext_ {
    _Bool with_links;
} RecordsOptParseCBContext;

static int records_opt_parse_cb(void * const context_,
                                const BinVal *key, const BinVal *value)
{
    RecordsOptParseCBContext * context = context_;
    char *svalue = value->val;    
    
    skip_spaces((const char * *) &svalue);
    if (*svalue == 0) {
        return 0;
    }
    if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, "links")) {
        char *endptr;
        _Bool with_links = (strtol(svalue, &endptr, 10) > 0);
        if (endptr == NULL || endptr == svalue) {
            return -1;
        }
        context->with_links = with_links;
                
        return 0;
    }
    return 0;
}

int handle_domain_records(struct evhttp_request * const req,
                          HttpHandlerContext * const context,
                          char *uri, char *opts, _Bool * const write_to_log,
                          const _Bool fake_req)
{
    Key *layer_name;
    Key *key;
    
    (void) opts;
    if (req->type == EVHTTP_REQ_GET) {
        Op op;
        RecordsGetOp * const get_op = &op.records_get_op;
        char *sep;
        
        if ((sep = strchr(uri, '/')) == NULL) {
            return HTTP_NOTFOUND;
        }
        *sep = 0;
        if ((layer_name = new_key_from_c_string(uri)) == NULL) {
            *sep = '/';
            return HTTP_SERVUNAVAIL;
        }
        *sep++ = '/';
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_NOTFOUND;
        }
        if ((key = new_key_from_c_string(sep)) == NULL) {
            release_key(layer_name);
            return HTTP_SERVUNAVAIL;
        }
        RecordsOptParseCBContext cb_context = {
            .with_links = 0
        };
        if (opts != NULL &&
            query_parse(opts, records_opt_parse_cb, &cb_context) != 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        *get_op = (RecordsGetOp) {
            .type = OP_TYPE_RECORDS_GET,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name,
            .key = key,
            .with_links = cb_context.with_links
        };        
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, get_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);            
            release_key(layer_name);
            release_key(key);        
            
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        
        return 0;
    }
    
    if (req->type == EVHTTP_REQ_PUT) {
        Op op;
        RecordsPutOp * const put_op = &op.records_put_op;
        char *sep;
        
        if ((sep = strchr(uri, '/')) == NULL) {
            return HTTP_NOTFOUND;
        }
        *sep = 0;
        if ((layer_name = new_key_from_c_string(uri)) == NULL) {
            return HTTP_SERVUNAVAIL;
        }
        *sep++ = '/';
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_NOTFOUND;
        }
        if ((key = new_key_from_c_string(sep)) == NULL) {
            release_key(layer_name);            
            return HTTP_SERVUNAVAIL;
        }
        evbuffer_add(evhttp_request_get_input_buffer(req), "", (size_t) 1U);
        const char *body =
            (char *) evbuffer_pullup(evhttp_request_get_input_buffer(req), -1);
        
        *put_op = (RecordsPutOp) {
            .type = OP_TYPE_RECORDS_PUT,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name,
            .key = key,
            .position = {
                .latitude  = (Dimension) -1,
                .longitude = (Dimension) -1
            },
            .position_set = 0,
            .properties = NULL,
            .special_properties = NULL,
            .expires_at = (time_t) 0
        };
        RecordsPutOptParseCBContext cb_context = {
            .put_op = put_op
        };
        if (query_parse(body, records_put_opt_parse_cb, &cb_context) != 0) {
            free_slip_map(&put_op->properties);
            free_slip_map(&put_op->special_properties);
            release_key(layer_name);
            release_key(key);
            return HTTP_BADREQUEST;
        }
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, put_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);            
            free_slip_map(&put_op->properties);
            free_slip_map(&put_op->special_properties);            
            release_key(layer_name);
            release_key(key);
            
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        *write_to_log = 1;
        
        return 0;
    }
    
    if (req->type == EVHTTP_REQ_DELETE) {
        Op op;
        RecordsDeleteOp * const delete_op = &op.records_delete_op;
        char *sep;
        if ((sep = strchr(uri, '/')) == NULL) {
            return HTTP_NOTFOUND;
        }
        *sep = 0;
        if ((layer_name = new_key_from_c_string(uri)) == NULL) {
            return HTTP_SERVUNAVAIL;
        }
        *sep++ = '/';
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_NOTFOUND;
        }
        if ((key = new_key_from_c_string(sep)) == NULL) {
            release_key(layer_name);            
            return HTTP_SERVUNAVAIL;
        }
        *delete_op = (RecordsDeleteOp) {
            .type = OP_TYPE_RECORDS_DELETE,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name,
            .key = key
        };
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, delete_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);            
            release_key(layer_name);
            release_key(key);
            
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        *write_to_log = 1;
        
        return 0;
    }    
    return HTTP_NOTFOUND;
}

typedef struct RecordsPutMergePropertiesCBContext_ {
    SlipMap * * target_map_pnt;
} RecordsPutMergePropertiesCBContext;

static int records_put_merge_properties_cb(void * const context_,
                                           const void * const key,
                                           const size_t key_len,
                                           const void * const value,
                                           const size_t value_len)
{
    RecordsPutMergePropertiesCBContext * const context = context_;
    
    assert(key_len > (size_t) 0U);
    assert(* (const char *) key != INT_PROPERTY_COMMON_PREFIX);
    replace_entry_in_slip_map(context->target_map_pnt,
                              (void *) key, key_len,
                              (void *) value, value_len);
    return 0;
}

typedef struct RecordsPutApplySpecialPropertiesCBContext_ {
    SlipMap * * target_map_pnt;
} RecordsPutApplySpecialPropertiesCBContext;

static int records_put_apply_special_properties_cb(void * const context_,
                                                   const void * const key,
                                                   const size_t key_len,
                                                   const void * const value,
                                                   const size_t value_len)
{
    RecordsPutApplySpecialPropertiesCBContext * const context = context_;
    
    assert(key_len > (size_t) 0U);
    assert(* (const char *) key == INT_PROPERTY_COMMON_PREFIX);
    
    if (key_len >= sizeof INT_PROPERTY_DELETE_PREFIX &&
        strncasecmp(key, INT_PROPERTY_DELETE_PREFIX,
                    sizeof INT_PROPERTY_DELETE_PREFIX - (size_t) 1U) == 0) {
        if (value_len <= (size_t) 0U || * (const char *) value == 0 ||
            context->target_map_pnt == NULL ||
            *context->target_map_pnt == NULL) {
            return 0;        
        }
        remove_entry_from_slip_map
            (context->target_map_pnt,
                (void *)
                ((const unsigned char *) key +
                    (sizeof INT_PROPERTY_DELETE_PREFIX - (size_t) 1U)),
                key_len - (sizeof INT_PROPERTY_DELETE_PREFIX - (size_t) 1U));
        return 0;
    }
    if (key_len == sizeof INT_PROPERTY_DELETE_ALL_PREFIX - (size_t) 1U &&
        strncasecmp(key, INT_PROPERTY_DELETE_ALL_PREFIX,
                    sizeof INT_PROPERTY_DELETE_ALL_PREFIX - (size_t) 1U) == 0) {
        if (value_len <= (size_t) 0U || * (const char *) value == 0 ||
            context->target_map_pnt == NULL ||
            *context->target_map_pnt == NULL) {
            return 0;        
        }
        free_slip_map(context->target_map_pnt);
        assert(*context->target_map_pnt == NULL);
        
        return 0;
    }
    
    if (key_len >= sizeof INT_PROPERTY_ADD_INT_PREFIX &&
        strncasecmp(key, INT_PROPERTY_ADD_INT_PREFIX,
                    sizeof INT_PROPERTY_ADD_INT_PREFIX - (size_t) 1U) == 0) {
        if (value_len <= (size_t) 0U || * (const char *) value == 0 ||
            value_len >= sizeof "-170141183460469231731687303715884105728") {
            return 0;        
        }
        const void *num_value_s = NULL;
        size_t num_value_len;
        if (find_in_slip_map
            (context->target_map_pnt,
                (void *) ((const unsigned char *) key +
                          (sizeof INT_PROPERTY_ADD_INT_PREFIX - (size_t) 1U)),
                key_len - (sizeof INT_PROPERTY_ADD_INT_PREFIX - (size_t) 1U),
                &num_value_s, &num_value_len) == 0) {
            num_value_s = "0";
            num_value_len = sizeof "0" - (size_t) 1U;
        }
        if (num_value_len >=
            sizeof "-170141183460469231731687303715884105728") {
            return 0;
        }        
        intmax_t num_value;
        char buf[num_value_len  + (size_t) 1U];        
        memcpy(buf, num_value_s, num_value_len);
        buf[num_value_len] = 0;
        num_value = strtoimax(buf, NULL, 10);

        intmax_t inum_value;
        char ibuf[value_len  + (size_t) 1U];        
        memcpy(ibuf, value, value_len);
        ibuf[value_len] = 0;
        inum_value = strtoimax(ibuf, NULL, 10);
        
        num_value += inum_value;
        
        char buf_s[sizeof "170141183460469231731687303715884105728"];
        snprintf(buf_s, sizeof buf_s, "%" PRIdMAX, num_value);
        if (*context->target_map_pnt == NULL) {
            *context->target_map_pnt = new_slip_map
                (PROPERTIES_DEFAULT_SLIP_MAP_BUFFER_SIZE);
            if (*context->target_map_pnt == NULL) {
                return -1;
            }
        }
        replace_entry_in_slip_map
            (context->target_map_pnt,
                (void *) ((const unsigned char *) key +
                          (sizeof INT_PROPERTY_ADD_INT_PREFIX - (size_t) 1U)),
                key_len - (sizeof INT_PROPERTY_ADD_INT_PREFIX - (size_t) 1U),
                buf_s, strlen(buf_s));
        
        return 0;
    }

    return 0;
}

int handle_op_records_put(RecordsPutOp * const put_op,
                          HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;

    if (get_pan_db_by_layer_name(context, put_op->layer_name->val,
                                 AUTOMATICALLY_CREATE_LAYERS, &pan_db) < 0) {
        release_key(put_op->layer_name);
        release_key(put_op->key);
        free_slip_map(&put_op->properties);
        free_slip_map(&put_op->special_properties);        
        
        return HTTP_NOTFOUND;
    }
    release_key(put_op->layer_name);

    int status;    
    KeyNode *key_node;
    status = get_key_node_from_key(pan_db, put_op->key, 1, &key_node);
    release_key(put_op->key);
    if (key_node == NULL) {
        assert(status <= 0);
        free_slip_map(&put_op->properties);
        free_slip_map(&put_op->special_properties);        
        return HTTP_NOTFOUND;
    }
    assert(status > 0);
    if (status > 0 && put_op->position_set != 0 && key_node->slot != NULL) {
#if PROJECTION
        const Position2D * const previous_position =
            &key_node->slot->real_position;
#else
        const Position2D * const previous_position =
            &key_node->slot->position;
#endif
        if (previous_position->latitude == put_op->position.latitude &&
            previous_position->longitude == put_op->position.longitude) {
            put_op->position_set = 0;
        } else {
            remove_entry_from_key_node(pan_db, key_node, 0);
            assert(key_node->slot != NULL);
            key_node->slot = NULL;
        }
    }
    if (put_op->position_set != 0) {
        Slot slot;
        Slot *new_slot;
        
        init_slot(&slot);
        slot = (Slot) {
#if PROJECTION
            .real_position = put_op->position,
#endif
            .position = put_op->position,
            .key_node = key_node
        };
        if (add_slot(pan_db, &slot, &new_slot) != 0) {
            RB_REMOVE(KeyNodes_, &pan_db->key_nodes, key_node);
            key_node->slot = NULL;
            free_key_node(pan_db, key_node);
            free_slip_map(&put_op->properties);
            free_slip_map(&put_op->special_properties);            
            
            return HTTP_SERVUNAVAIL;
        }
        key_node->slot = new_slot;
        assert(new_slot != NULL);
    }
    if (put_op->special_properties != NULL) {
        RecordsPutApplySpecialPropertiesCBContext cb_context = {
            .target_map_pnt = &key_node->properties
        };
        slip_map_foreach(&put_op->special_properties,
                         records_put_apply_special_properties_cb,
                         &cb_context);
        free_slip_map(&put_op->special_properties);
    }
    if (key_node->properties == NULL) {
        key_node->properties = put_op->properties;
    } else {    
        RecordsPutMergePropertiesCBContext cb_context = {
            .target_map_pnt = &key_node->properties
        };
        slip_map_foreach(&put_op->properties, records_put_merge_properties_cb,
                         &cb_context);
        free_slip_map(&put_op->properties);
    }
    Expirable *expirable = key_node->expirable;
    if (put_op->expires_at != (time_t) 0) {
        if (expirable == NULL) {
            Expirable new_expirable = {
                .ts = put_op->expires_at,
                .key_node = key_node
            };
            expirable = add_entry_to_slab(&context->expirables_slab,
                                          &new_expirable);
            key_node->expirable = expirable;
            add_expirable_to_tree(pan_db, expirable);
        } else {
            if (expirable->ts != put_op->expires_at) {
                remove_expirable_from_tree(pan_db, expirable);
                expirable->ts = put_op->expires_at;
                add_expirable_to_tree(pan_db, expirable);
            }
        }
        assert(expirable->key_node == key_node);
    } else if (expirable != NULL) {
        assert(expirable->key_node == key_node);
        remove_expirable_from_tree(pan_db, expirable);        
        remove_entry_from_slab(&context->expirables_slab, expirable);
        key_node->expirable = NULL;
    }
    if (put_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    RecordsPutOpReply * const put_op_reply =
        &op_reply->records_put_op_reply;
    
    *put_op_reply = (RecordsPutOpReply) {
        .type = OP_TYPE_RECORDS_PUT,
        .req = put_op->req,
        .op_tid = put_op->op_tid,
        .json_gen = NULL                        
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    put_op_reply->json_gen = json_gen;
    yajl_gen_string(json_gen, (const unsigned char *) "status",
                    (unsigned int) sizeof "status" - (size_t) 1U);
    yajl_gen_string(json_gen, (const unsigned char *) "stored",
                    (unsigned int) sizeof "stored" - (size_t) 1U);
    send_op_reply(context, op_reply);
    
    return 0;
}

int handle_op_records_get(RecordsGetOp * const get_op,
                          HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;

    if (get_pan_db_by_layer_name(context, get_op->layer_name->val,
                                 AUTOMATICALLY_CREATE_LAYERS, &pan_db) < 0) {
        release_key(get_op->layer_name);
        release_key(get_op->key);
        
        return HTTP_NOTFOUND;
    }
    release_key(get_op->layer_name);

    int status;
    KeyNode *key_node;
    status = get_key_node_from_key(pan_db, get_op->key, 0, &key_node);
    release_key(get_op->key);
    if (key_node == NULL) {
        assert(status <= 0);
        return HTTP_NOTFOUND;
    }
    assert(status > 0);
    if (get_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    RecordsGetOpReply * const get_op_reply =
        &op_reply->records_get_op_reply;
    
    *get_op_reply = (RecordsGetOpReply) {
        .type = OP_TYPE_RECORDS_GET,
        .req = get_op->req,
        .op_tid = get_op->op_tid,
        .json_gen = NULL                        
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    get_op_reply->json_gen = json_gen;
    key_node_to_json(key_node, json_gen, pan_db, 1, get_op->with_links);
    send_op_reply(context, op_reply);
    
    return 0;
}

int handle_op_records_delete(RecordsDeleteOp * const delete_op,
                             HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;

    if (get_pan_db_by_layer_name(context, delete_op->layer_name->val, 0,
                                 &pan_db) < 0) {
        release_key(delete_op->layer_name);
        release_key(delete_op->key);
        
        return HTTP_NOTFOUND;
    }
    release_key(delete_op->layer_name);

    int status;
    KeyNode *key_node;
    status = get_key_node_from_key(pan_db, delete_op->key, 0, &key_node);
    release_key(delete_op->key);
    if (key_node == NULL) {
        assert(status <= 0);
        return HTTP_NOTFOUND;
    }
    assert(status > 0);
    if (key_node->slot != NULL) {
        if (remove_entry_from_key_node(pan_db, key_node, 0) != 0) {
            return HTTP_SERVUNAVAIL;
        }
        key_node->slot = NULL;        
    }
    assert(key_node->slot == NULL);
    RB_REMOVE(KeyNodes_, &pan_db->key_nodes, key_node);
    free_key_node(pan_db, key_node);
    if (delete_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    RecordsDeleteOpReply * const delete_op_reply =
        &op_reply->records_delete_op_reply;
            
    *delete_op_reply = (RecordsDeleteOpReply) {
        .type = OP_TYPE_RECORDS_DELETE,
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
