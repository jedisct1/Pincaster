
#ifndef __DOMAIN_PUBLIC_H__
#define __DOMAIN_PUBLIC_H__ 1

int handle_public_request(HttpHandlerContext * const context,
                          const char * const decoded_uri,
                          const char * const opts,
                          struct evhttp_request * const req);

#endif
