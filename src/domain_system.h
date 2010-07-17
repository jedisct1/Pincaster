
#ifndef __DOMAIN_SYSTEM_H__
#define __DOMAIN_SYSTEM_H__ 1

#define TMP_LOG_BUFFER_SIZE 65536
#ifndef BGREWRITEAOF_NICENESS
# define BGREWRITEAOF_NICENESS 10
#endif

int handle_domain_system(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req);

int handle_op_system_ping(SystemPingOp * const ping_op,
                          HttpHandlerContext * const context);

int system_rewrite_after_fork_cb(HttpHandlerContext * const context);

char *get_tmp_log_file_name(void);

#endif
