
#ifndef __EXPIRABLES_H__
#define __EXPIRABLES_H__ 1

int add_expirable_to_tree(PanDB * const pan_db, Expirable * const expirable);

int remove_expirable_from_tree(PanDB * const pan_db,
                               Expirable * const expirable);

int purge_expired_keys(HttpHandlerContext * const context);

#endif
