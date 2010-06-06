
#include "common.h"
#include "http_server.h"
#include "public.h"

int handle_public_request(HttpHandlerContext * const context,
                          const char * const decoded_uri,
                          const char * const opts,
                          struct evhttp_request * const req)
{
    (void) context;
    (void) decoded_uri;
    (void) opts;
    
    evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
    
    return -1;   
}

