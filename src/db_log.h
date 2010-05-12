
#ifndef __DBLOG_H__
#define __DBLOG_H__ 1

#define DB_LOG_RECORD_COOKIE_HEAD "-<"
#define DB_LOG_RECORD_COOKIE_TAIL ">\n"
#ifndef DB_LOG_MAX_URI_LEN
# define DB_LOG_MAX_URI_LEN  (size_t) 10000U
#endif
#ifndef DB_LOG_MAX_BODY_LEN
# define DB_LOG_MAX_BODY_LEN (size_t) 100000U
#endif
#ifndef DB_LOG_TMP_SUFFIX
# define DB_LOG_TMP_SUFFIX ".tmp"
#endif

typedef struct DBLog_ {
    char *db_log_file_name;
    int db_log_fd;
    struct evbuffer *log_buffer;
    size_t journal_buffer_size;
    int fsync_period;
    pid_t journal_rewrite_process;
    off_t offset_before_fork;
} DBLog;

int init_db_log(void);
int open_db_log(void);
void free_db_log(void);
int close_db_log(void);
int add_to_db_log(const int verb,
                  const char *uri, struct evbuffer * const input_buffer);
int flush_db_log(const _Bool sync);

#endif
