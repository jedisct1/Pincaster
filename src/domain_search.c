
#include "common.h"
#include "http_server.h"
#include "domain_layers.h"
#include "domain_search.h"
#include "query_parser.h"

#define DEFAULT_SEARCH_LIMIT 250

typedef struct SearchOptParseCBContext_ {
    Dimension radius;
    SubSlots limit;
    _Bool with_properties;
} SearchOptParseCBContext;

static int search_opt_parse_cb(void * const context_,
                               const BinVal *key, const BinVal *value)
{
    SearchOptParseCBContext * context = context_;
    char *svalue = value->val;    
    
    skip_spaces((const char * *) &svalue);
    if (*svalue == 0) {
        return 0;
    }
    if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, "radius")) {
        char *endptr;
        Dimension radius = (Dimension) strtod(svalue, &endptr);
        if (endptr == NULL || endptr == svalue || radius < (Dimension) 0.0) {
            return -1;
        }
        context->radius = radius;
        return 0;
    }
    if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, "limit")) {
        char *endptr;
        SubSlots limit = (SubSlots) strtoul(svalue, &endptr, 10);
        if (endptr == NULL || endptr == svalue || limit <= (SubSlots) 0U) {
            return -1;
        }
        context->limit = limit;
        return 0;
    }
    if (BINVAL_IS_EQUAL_TO_CONST_STRING(key, "properties")) {
        char *endptr;
        unsigned long v = strtoul(svalue, &endptr, 10);
        if (endptr == NULL || endptr == svalue) {
            return -1;
        }
        if (v != 0) {
            context->with_properties = 1;
        } else {
            context->with_properties = 0;
        }
        return 0;
    }
    return 0;
}

int handle_domain_search(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req)
{
    (void) write_to_log;
    
    if (req->type != EVHTTP_REQ_GET) {
        return HTTP_NOTFOUND;
    }
    Key *layer_name;
    Op op;
    char *sep;
    char *search_type;
    char *query;
    char *zeroed1, *zeroed2;

    (void) opts;
    if ((sep = strchr(uri, '/')) == NULL) {
        return HTTP_NOTFOUND;
    }
    *sep = 0;
    if (*uri == 0 || (layer_name = new_key_from_c_string(uri)) == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    *sep = '/';
    sep++;
    search_type = sep;
    if ((sep = strchr(search_type, '/')) == NULL) {
        release_key(layer_name);        
        return HTTP_NOTFOUND;
    }
    zeroed1 = sep;
    *sep++ = 0;
    if (*sep == 0) {
        release_key(layer_name);
        return HTTP_NOTFOUND;
    }
    SearchOptParseCBContext cb_context = {
        .radius = (Dimension) 0.0,
        .limit = DEFAULT_SEARCH_LIMIT,
        .with_properties = 1
    };
    if (opts != NULL &&
        query_parse(opts, search_opt_parse_cb, &cb_context) != 0) {
        release_key(layer_name);
        return HTTP_BADREQUEST;
    }
    query = sep;
    if (strcasecmp(search_type, "nearby") == 0) {
        SearchNearbyOp * const nearby_op = &op.search_nearby_op;

        *zeroed1 = '/';
        *nearby_op = (SearchNearbyOp) {
            .type = OP_TYPE_SEARCH_NEARBY,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name,
            .position = {
                .latitude  = (Dimension) -1,
                .longitude = (Dimension) -1
            },
            .radius = cb_context.radius,
            .limit = cb_context.limit,
            .with_properties = cb_context.with_properties
        };
        if (*query == 0 || (sep = strchr(query, ',')) == NULL) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        zeroed2 = sep;
        *sep++ = 0;
        skip_spaces((const char * *) &sep);
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        char *endptr;
        nearby_op->position.latitude = (Dimension) strtod(query, &endptr);
        if (endptr == NULL || endptr == query) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        nearby_op->position.longitude = (Dimension) strtod(sep, &endptr);
        if (endptr == NULL || endptr == sep) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        *zeroed2 = ',';
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, nearby_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            release_key(layer_name);
            
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        return 0;
    }

    if (strcasecmp(search_type, "in_rect") == 0) {
        SearchInRectOp * const in_rect_op = &op.search_in_rect_op;
        
        *zeroed1 = '/';
        *in_rect_op = (SearchInRectOp) {
            .type = OP_TYPE_SEARCH_IN_RECT,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid,
            .layer_name = layer_name,
            .rect = { { 0, 0 }, { 0, 0 } },
            .limit = cb_context.limit,
            .with_properties = cb_context.with_properties
         };
        
        if (*query == 0 || (sep = strchr(query, ',')) == NULL) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        zeroed2 = sep;
        *sep++ = 0;
        skip_spaces((const char * *) &sep);
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        char *endptr;
        in_rect_op->rect.edge0.latitude = (Dimension) strtod(query, &endptr);
        if (endptr == NULL || endptr == query) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        *zeroed2 = ',';
        
        query = sep;
        skip_spaces((const char * *) &query);
        if (*query == 0 || (sep = strchr(query, ',')) == NULL) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        zeroed2 = sep;
        *sep++ = 0;
        skip_spaces((const char * *) &sep);
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        in_rect_op->rect.edge0.longitude = (Dimension) strtod(query, &endptr);
        if (endptr == NULL || endptr == query) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        *zeroed2 = ',';
        
        query = sep;
        skip_spaces((const char * *) &query);
        if (*query == 0 || (sep = strchr(query, ',')) == NULL) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        zeroed2 = sep;
        *sep++ = 0;
        skip_spaces((const char * *) &sep);
        if (*sep == 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        in_rect_op->rect.edge1.latitude = (Dimension) strtod(query, &endptr);
        if (endptr == NULL || endptr == query) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        *zeroed2 = ',';

        query = sep;
        skip_spaces((const char * *) &query);
        if (*query == 0) {
            release_key(layer_name);
            return HTTP_BADREQUEST;
        }
        in_rect_op->rect.edge1.longitude = (Dimension) strtod(query, &endptr);
        if (endptr == NULL || endptr == query) {
            release_key(layer_name);            
            return HTTP_BADREQUEST;
        }
        pthread_mutex_lock(&context->mtx_cqueue);
        if (push_cqueue(context->cqueue, in_rect_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            release_key(layer_name);
            
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        return 0;
    }
    return HTTP_NOTFOUND;
}

typedef struct FindNearCBContext_ {
    PanDB *pan_db;
    yajl_gen json_gen;
    _Bool with_properties;
} FindNearCBContext;

static int find_near_cb(void * const context_,
                        Slot * const slot, const Meters distance)
{
    FindNearCBContext * const context = context_;
    yajl_gen json_gen = context->json_gen;
    KeyNode * const key_node = slot->key_node;
    
    assert(key_node != NULL);
    yajl_gen_map_open(json_gen);
    yajl_gen_string(json_gen,
                    (const unsigned char *) "distance",
                    (unsigned int) sizeof "distance" - (size_t) 1U);
    yajl_gen_double(json_gen, distance);
    key_node_to_json(key_node, json_gen, context->with_properties);
    yajl_gen_map_close(json_gen);
    
    return 0;
}

int handle_op_search_nearby(SearchNearbyOp * const nearby_op,
                            HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;
        
    if (get_pan_db_by_layer_name(context, nearby_op->layer_name->val,
                                 AUTOMATICALLY_CREATE_LAYERS, &pan_db) < 0) {
        assert(pan_db == NULL);        
        release_key(nearby_op->layer_name);
        
        return HTTP_NOTFOUND;
    }
    release_key(nearby_op->layer_name);
    
    if (nearby_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    SearchNearbyOpReply * const nearby_op_reply =
        &op_reply->search_nearby_op_reply;
    
    *nearby_op_reply = (SearchNearbyOpReply) {
        .type = OP_TYPE_SEARCH_NEARBY,
        .req = nearby_op->req,         
        .op_tid = nearby_op->op_tid,
        .json_gen = NULL
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }        
    nearby_op_reply->json_gen = json_gen;        
    yajl_gen_string(json_gen,
                    (const unsigned char *) "matches",
                    (unsigned int) sizeof "matches" - (size_t) 1U);
    yajl_gen_array_open(json_gen);
    
    FindNearCBContext cb_context = {
        .pan_db = pan_db,
        .json_gen = json_gen,
        .with_properties = nearby_op->with_properties
    };
    find_near(pan_db, find_near_cb, &cb_context, &nearby_op->position,
              nearby_op->radius, nearby_op->limit);

    yajl_gen_array_close(json_gen);
    
    send_op_reply(context, op_reply);
    
    return 0;
}

int handle_op_search_in_rect(SearchInRectOp * const in_rect_op,
                             HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    PanDB *pan_db;
        
    if (get_pan_db_by_layer_name(context, in_rect_op->layer_name->val,
                                 AUTOMATICALLY_CREATE_LAYERS, &pan_db) < 0) {
        assert(pan_db == NULL);        
        release_key(in_rect_op->layer_name);
        
        return HTTP_NOTFOUND;
    }
    release_key(in_rect_op->layer_name);
    
    if (in_rect_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    SearchInRectOpReply * const in_rect_op_reply =
        &op_reply->search_in_rect_op_reply;
    
    *in_rect_op_reply = (SearchInRectOpReply) {
        .type = OP_TYPE_SEARCH_IN_RECT,
        .req = in_rect_op->req,         
        .op_tid = in_rect_op->op_tid,
        .json_gen = NULL
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }        
    in_rect_op_reply->json_gen = json_gen;        
    yajl_gen_string(json_gen,
                    (const unsigned char *) "matches",
                    (unsigned int) sizeof "matches" - (size_t) 1U);
    yajl_gen_array_open(json_gen);
    
    FindNearCBContext cb_context = {
        .pan_db = pan_db,
        .json_gen = json_gen,
        .with_properties = in_rect_op->with_properties
    };
    find_in_rect(pan_db, find_near_cb, &cb_context, &in_rect_op->rect,
                 in_rect_op->limit);

    yajl_gen_array_close(json_gen);
    
    send_op_reply(context, op_reply);
    
    return 0;
}