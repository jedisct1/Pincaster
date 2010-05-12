
#ifndef __DOMAIN_RECORDS_H__
#define __DOMAIN_RECORDS_H__ 1

#define INT_PROPERTY_COMMON_PREFIX     '_'
#define INT_PROPERTY_TYPE              "_type"
#define INT_PROPERTY_EXPIRES_AT        "_expires_at"
#define INT_PROPERTY_TTL               "_ttl"
#define INT_PROPERTY_POSITION          "_loc"
#define INT_PROPERTY_DELETE_PREFIX     "_delete:"
#define INT_PROPERTY_DELETE_ALL_PREFIX "_delete_all"
#define INT_PROPERTY_ADD_INT_PREFIX    "_add_int:"

int handle_domain_records(struct evhttp_request * const req,
                          HttpHandlerContext * const context,
                          char *uri, char *opts, _Bool * const write_to_log,
                          const _Bool fake_req);

int handle_op_records_put(RecordsPutOp * const put_op,
                          HttpHandlerContext * const context);

int handle_op_records_get(RecordsGetOp * const get_op,
                          HttpHandlerContext * const context);

int handle_op_records_delete(RecordsDeleteOp * const get_op,
                             HttpHandlerContext * const context);

#endif
