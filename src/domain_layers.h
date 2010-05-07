
#ifndef __DOMAIN_LAYERS_H__
#define __DOMAIN_LAYERS_H__ 1

int handle_domain_layers(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req);

int handle_op_layers_create(LayersCreateOp * const create_op,
                            HttpHandlerContext * const context);

int handle_op_layers_delete(LayersDeleteOp * const delete_op,
                            HttpHandlerContext * const context);

int handle_op_layers_index(LayersIndexOp * const index_op,
                            HttpHandlerContext * const context);

int get_pan_db_by_layer_name(HttpHandlerContext * const context_,
                             const char * const layer_name,
                             const _Bool create,
                             PanDB * * const pan_db);

#endif
