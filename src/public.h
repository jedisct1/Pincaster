
#ifndef __DOMAIN_PUBLIC_H__
#define __DOMAIN_PUBLIC_H__ 1

#ifndef DEFAULT_CONTENT_TYPE_FOR_PUBLIC_DATA
# define DEFAULT_CONTENT_TYPE_FOR_PUBLIC_DATA "text/plain"
#endif

int handle_public_request(HttpHandlerContext * const context,
                          const char * const uri,
                          const char * const opts,
                          struct evhttp_request * const req);

#endif
