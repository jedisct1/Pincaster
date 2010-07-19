
#ifndef __DBLOG_H__
#define __DBLOG_H__ 1

#define DB_LOG_RECORD_COOKIE_HEAD "-<"
#define DB_LOG_RECORD_COOKIE_TAIL ">\n"
#define DB_LOG_RECORD_COOKIE_MARK_CHAR "@"
#define DB_LOG_RECORD_COOKIE_TIMESTAMP_CHAR "T"
#ifndef DB_LOG_MAX_URI_LEN
# define DB_LOG_MAX_URI_LEN  (size_t) 10000U
#endif
#ifndef DB_LOG_MAX_BODY_LEN
# define DB_LOG_MAX_BODY_LEN (size_t) 100000U
#endif
#ifndef DB_LOG_TMP_SUFFIX
# define DB_LOG_TMP_SUFFIX ".tmp"
#endif
#ifndef DB_LOG_TIMESTAMP_GRANULARITY
# define DB_LOG_TIMESTAMP_GRANULARITY (time_t) 60
#endif

typedef struct DBLog_ {
    char *db_log_file_name;
    int db_log_fd;
    struct evbuffer *log_buffer;
    size_t journal_buffer_size;
    int fsync_period;
    pid_t journal_rewrite_process;
    off_t offset_before_fork;
    time_t last_ts;
} DBLog;

int init_db_log(void);
int open_db_log(void);
void free_db_log(void);
int close_db_log(void);

int add_to_db_log(struct HttpHandlerContext_ * const context, const int verb,
                  const char *uri, struct evbuffer * const input_buffer,
                  _Bool send_to_slaves);

int flush_db_log(const _Bool sync);

int add_ts_to_ev_log_buffer(struct evbuffer * const log_buffer,
                            const time_t ts);

int reset_log(struct HttpHandlerContext_ * const context);

#endif
