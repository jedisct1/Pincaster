
#ifndef __LOG_H__
#define __LOG_H__ 1

#ifndef MAX_LOG_LINE
# define MAX_LOG_LINE 1024U
#endif
#ifndef LOG_WRITE_TIMEOUT
# define LOG_WRITE_TIMEOUT 10
#endif

#ifdef DEBUG
# define XDEBUG(X) do { X } while(0)
#else
# define XDEBUG(X)
#endif

int logfile(struct HttpHandlerContext_ * const context,
            const int crit, const char * const format, ...)
__attribute__ ((format(printf, 3, 4)));

int logfile_noformat(struct HttpHandlerContext_ * const context,
                     const int crit, const char * const msg);

int logfile_error(struct HttpHandlerContext_ * const context,
                  const char * const msg);

#endif
