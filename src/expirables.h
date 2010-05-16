
#ifndef __EXPIRABLES_H__
#define __EXPIRABLES_H__ 1

#ifndef SPREAD_EXPIRATION
# define SPREAD_EXPIRATION 0
#endif

int add_expirable_to_tree(PanDB * const pan_db, Expirable * const expirable);

int remove_expirable_from_tree(PanDB * const pan_db,
                               Expirable * const expirable);

int purge_expired_keys(HttpHandlerContext * const context);

#endif
