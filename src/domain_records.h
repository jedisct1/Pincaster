
#ifndef __DOMAIN_RECORDS_H__
#define __DOMAIN_RECORDS_H__ 1

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
