
#include "common.h"
#include "http_server.h"
#include "domain_layers.h"
#include "public.h"

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
    if (req->type != EVHTTP_REQ_GET || (sep = strchr(uri, '/')) == NULL) {
        evhttp_add_header(req->output_headers, "Allow", "GET");
        evhttp_send_error(req, 405, "Method not allowed");
        return -1;
    }
    *sep = 0;
    if ((layer_name = new_key_from_c_string(uri)) == NULL) {
        *sep = '/';
        evhttp_send_error(req, HTTP_SERVUNAVAIL, "Out of memory (layer)");
        return -1;
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
        return -1;
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
    if (key_node == NULL || status < 0) {
        goto unlock_and_bailout;
    }
    pthread_rwlock_unlock(&context->rwlock_layers);
    evhttp_send_error(req, HTTP_NOTFOUND, "Not Found (in progress)");
    
    return -1;   
}

