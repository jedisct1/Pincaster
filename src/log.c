
#include "common.h"
#include "http_server.h"
#include "log.h"

int logfile(struct HttpHandlerContext_ * const context,
            const int crit, const char * const format, ...)
{
    const char *urgency;    
    va_list va;
    char line[MAX_LOG_LINE];

#ifndef DEBUG
    if (crit == LOG_DEBUG) {
        return 0;
    }
#endif
    switch (crit) {
    case LOG_INFO:
        urgency = "[INFO] ";
        break;
    case LOG_WARNING:
        urgency = "[WARNING] ";
        break;
    case LOG_ERR:
        urgency = "[ERROR] ";
        break;
    case LOG_NOTICE:
        urgency = "[NOTICE] ";
        break;
    case LOG_DEBUG:
        urgency = "[DEBUG] ";
        break;
    default:
        urgency = "";
    }
    va_start(va, format);
    vsnprintf(line, sizeof line, format, va);
    va_end(va);
    
    int log_fd;    
    if (context == NULL || context->log_fd == -1) {
        log_fd = STDERR_FILENO;
    } else {
        log_fd = context->log_fd;
    }
    safe_write(log_fd, urgency, strlen(urgency), LOG_WRITE_TIMEOUT);
    safe_write(log_fd, line, strlen(line), LOG_WRITE_TIMEOUT);
    safe_write(log_fd, "\n", (size_t) 1U, LOG_WRITE_TIMEOUT);

    return 0;
}

int logfile_noformat(struct HttpHandlerContext_ * const context,
                     const int crit, const char * const msg)
{
    return logfile(context, crit, "%s", msg);
}

int logfile_error(struct HttpHandlerContext_ * const context,
                  const char * const msg)
{
    const char *const err_msg = strerror(errno);
    
    return logfile(context, LOG_ERR, "%s: %s", msg, err_msg);
}
