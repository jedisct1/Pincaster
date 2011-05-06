
#include "common.h"
#include "http_server.h"
#include "handle_consumer_ops.h"

static int send_json_gen(yajl_gen json_gen, const OpReply * const op_reply)
{
    const unsigned char *json_out_buf;
    size_t json_out_len;
    struct evbuffer *evb;
    
    yajl_gen_map_close(json_gen);        
    yajl_gen_get_buf(json_gen, &json_out_buf, &json_out_len);
    if ((evb = evbuffer_new()) == NULL) {
        yajl_gen_free(json_gen);
        return -1;
    }
    evbuffer_add(evb, json_out_buf, json_out_len);
    evhttp_send_reply(op_reply->bare_op_reply.req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
    yajl_gen_free(json_gen);
    
    return 0;
}

static int handle_consumer_op_error(OpReply * const op_reply)
{
    ErrorOpReply * const error_op_reply = &op_reply->error_op_reply;
    evhttp_send_error(error_op_reply->req, HTTP_NOTFOUND, "Not Found");
    
    return 0;
}

static int handle_consumer_op_system_ping(OpReply * const op_reply)
{
    SystemPingOpReply * const system_ping_op_reply =
        &op_reply->system_ping_op_reply;
    yajl_gen json_gen = system_ping_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_system_rewrite(OpReply * const op_reply)
{
    SystemRewriteOpReply * const system_rewrite_op_reply =
        &op_reply->system_rewrite_op_reply;
    yajl_gen json_gen = system_rewrite_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_layers_create(OpReply * const op_reply)
{
    LayersCreateOpReply * const layers_create_op_reply =
        &op_reply->layers_create_op_reply;
    yajl_gen json_gen = layers_create_op_reply->json_gen;
        
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_layers_delete(OpReply * const op_reply)
{
    LayersDeleteOpReply * const layers_delete_op_reply =
        &op_reply->layers_delete_op_reply;
    yajl_gen json_gen = layers_delete_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_layers_index(OpReply * const op_reply)
{
    LayersIndexOpReply * const layers_index_op_reply =
        &op_reply->layers_index_op_reply;
    yajl_gen json_gen = layers_index_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_records_put(OpReply * const op_reply)
{
    RecordsPutOpReply * const records_put_op_reply =
        &op_reply->records_put_op_reply;
    yajl_gen json_gen = records_put_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_records_get(OpReply * const op_reply)
{
    RecordsGetOpReply * const records_get_op_reply =
        &op_reply->records_get_op_reply;
    yajl_gen json_gen = records_get_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_records_delete(OpReply * const op_reply)
{
    RecordsDeleteOpReply * const records_delete_op_reply =
        &op_reply->records_delete_op_reply;
    yajl_gen json_gen = records_delete_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_search_nearby(OpReply * const op_reply)
{
    SearchNearbyOpReply * const search_nearby_op_reply =
        &op_reply->search_nearby_op_reply;
    yajl_gen json_gen = search_nearby_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_search_in_rect(OpReply * const op_reply)
{
    SearchInRectOpReply * const search_in_rect_op_reply =
        &op_reply->search_in_rect_op_reply;
    yajl_gen json_gen = search_in_rect_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

static int handle_consumer_op_search_in_keys(OpReply * const op_reply)
{
    SearchInKeysOpReply * const search_in_keys_op_reply =
        &op_reply->search_in_keys_op_reply;
    yajl_gen json_gen = search_in_keys_op_reply->json_gen;
    
    return send_json_gen(json_gen, op_reply);
}

void consumer_cb(struct bufferevent * const bev, void *context_)
{
    OpReply *op_reply;
    int ret = -1;

    (void) context_;
    while (evbuffer_get_length(bev->input) >= sizeof op_reply) {
        assert(bufferevent_read(bev, &op_reply, sizeof op_reply)
               == sizeof op_reply);
        switch (op_reply->bare_op_reply.type) {
        case OP_TYPE_ERROR:
            ret = handle_consumer_op_error(op_reply);
            break;
        case OP_TYPE_SYSTEM_PING:
            ret = handle_consumer_op_system_ping(op_reply);
            break;
        case OP_TYPE_SYSTEM_REWRITE:
            ret = handle_consumer_op_system_rewrite(op_reply);
            break;
        case OP_TYPE_LAYERS_CREATE:
            ret = handle_consumer_op_layers_create(op_reply);
            break;
        case OP_TYPE_LAYERS_DELETE:
            ret = handle_consumer_op_layers_delete(op_reply);
            break;
        case OP_TYPE_LAYERS_INDEX:
            ret = handle_consumer_op_layers_index(op_reply);
            break;
        case OP_TYPE_RECORDS_PUT:
            ret = handle_consumer_op_records_put(op_reply);
            break;
        case OP_TYPE_RECORDS_GET:
            ret = handle_consumer_op_records_get(op_reply);
            break;
        case OP_TYPE_RECORDS_DELETE:
            ret = handle_consumer_op_records_delete(op_reply);
            break;
        case OP_TYPE_SEARCH_NEARBY:
            ret = handle_consumer_op_search_nearby(op_reply);
            break;
        case OP_TYPE_SEARCH_IN_RECT:
            ret = handle_consumer_op_search_in_rect(op_reply);
            break;
        case OP_TYPE_SEARCH_IN_KEYS:
            ret = handle_consumer_op_search_in_keys(op_reply);
            break;
        default:
            ret = -1;
        }
        if (ret != 0) {
            evhttp_send_error(op_reply->bare_op_reply.req,
                              ret, "Error");
        }
        op_reply->bare_op_reply.type = OP_TYPE_NONE;
        op_reply->bare_op_reply.req = NULL;
        op_reply->bare_op_reply.op_tid = (OpTID) 0;
        free(op_reply);
    }
}

