
#include "common.h"
#include "http_server.h"
#include "domain_layers.h"
#include "domain_records.h"
#include "public.h"

typedef struct PublicPropertiesCBContext_ {
    const void *content;
    size_t content_len;
    const void *content_type;
    size_t content_type_len;
} PublicPropertiesCBContext;

static int public_properties_cb(void * const context_,
                                const void * const key,
                                const size_t key_len,
                                const void * const value,
                                const size_t value_len)
{
    PublicPropertiesCBContext * const context = context_;
    
    if (key_len == sizeof INT_PROPERTY_CONTENT - (size_t) 1U &&
        memcmp(key, INT_PROPERTY_CONTENT,
               sizeof INT_PROPERTY_CONTENT - (size_t) 1U) == 0) {
        context->content = value;
        context->content_len = value_len;
    } else if (key_len == sizeof INT_PROPERTY_CONTENT_TYPE - (size_t) 1U &&
               memcmp(key, INT_PROPERTY_CONTENT_TYPE,
                      sizeof INT_PROPERTY_CONTENT_TYPE - (size_t) 1U) == 0) {
        context->content_type = value;
        context->content_type_len = value_len;
    } else {
        return 0;
    }
    if (context->content_len > (size_t) 0U &&
        context->content_type_len > (size_t) 0U) {
        return 1;
    }
    return 0;
}

int handle_public_request(HttpHandlerContext * const context,
                          const char * const uri,
                          const char * const opts,
                          struct evhttp_request * const req)
{
    Key *layer_name;
    Key *key;
    PanDB *pan_db;
    KeyNode *key_node;
    int status;
    char *sep;    
    
    (void) opts;
    if (req->type != EVHTTP_REQ_GET) {
        evhttp_add_header(req->output_headers, "Allow", "GET");
        evhttp_send_error(req, 405, "Method not allowed");
        return -1;
    }
    if ((sep = strchr(uri, '/')) == NULL) {
        evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        return -1;        
    }
    *sep = 0;
    if ((layer_name = new_key_from_c_string(uri)) == NULL) {
        *sep = '/';
        evhttp_send_error(req, HTTP_SERVUNAVAIL, "Out of memory (layer)");
        return -2;
    }
    *sep++ = '/';
    if (*sep == 0) {
        release_key(layer_name);
        evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        return -1;
    }
    if ((key = new_key_from_c_string(sep)) == NULL) {
        release_key(layer_name);
        evhttp_send_error(req, HTTP_SERVUNAVAIL, "Out of memory (key)");
        return -2;
    }    
    pthread_rwlock_rdlock(&context->rwlock_layers);
    if (get_pan_db_by_layer_name(context, layer_name->val, 0, &pan_db) < 0) {
        release_key(layer_name);
        release_key(key);
        
unlock_and_bailout:
        pthread_rwlock_unlock(&context->rwlock_layers);
        evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        
        return -1;
    }
    release_key(layer_name);
    status = get_key_node_from_key(pan_db, key, 0, &key_node);
    release_key(key);
    if (key_node == NULL || status <= 0) {
        goto unlock_and_bailout;
    }
    PublicPropertiesCBContext cb_context = {
        .content = NULL,
        .content_len = (size_t) 0U,
        .content_type = NULL,
        .content_type_len = (size_t) 0U
    };
    slip_map_foreach(&key_node->properties, public_properties_cb,
                     &cb_context);
    if (cb_context.content_len <= (size_t) 0U) {
        pthread_rwlock_unlock(&context->rwlock_layers);
        evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        return -1;
    }
    if (cb_context.content_type_len > MAX_CONTENT_TYPE_LENGTH) {
        pthread_rwlock_unlock(&context->rwlock_layers);
        evhttp_send_error(req, HTTP_BADREQUEST, "Content-Type too long");
        return -1;        
    }
    struct evbuffer *evb;
    if ((evb = evbuffer_new()) == NULL) {
        pthread_rwlock_unlock(&context->rwlock_layers);
        evhttp_send_error(req, HTTP_SERVUNAVAIL, "Out of memory (evbuffer)");
        return -1;
    }
    const char *content_type;
    char content_type_buf[MAX_CONTENT_TYPE_LENGTH + (size_t) 1U];
    if (cb_context.content_type_len <= (size_t) 0U) {
        content_type = DEFAULT_CONTENT_TYPE_FOR_PUBLIC_DATA;
    } else {
        assert(cb_context.content_type_len <= MAX_CONTENT_TYPE_LENGTH);
        memcpy(content_type_buf, cb_context.content_type,
               cb_context.content_type_len);
        *(content_type_buf + cb_context.content_type_len) = 0;
        content_type = content_type_buf;
    }
    evhttp_add_header(req->output_headers, "Content-Type", content_type);
    evbuffer_add(evb, cb_context.content, cb_context.content_len);
    pthread_rwlock_unlock(&context->rwlock_layers);
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
    
    return -1;   
}

