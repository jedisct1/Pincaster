
#ifndef __DOMAIN_SEARCH_H__
#define __DOMAIN_SEARCH_H__ 1

int handle_domain_search(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req);

int handle_op_search_nearby(SearchNearbyOp * const nearby_op,
                            HttpHandlerContext * const context);

int handle_op_search_in_rect(SearchInRectOp * const in_rect_op,
                             HttpHandlerContext * const context);

#endif
