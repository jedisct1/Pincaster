
#include "common.h"
#include "http_server.h"
#include "replication_slave.h"

static int init_replication_context(ReplicationSlaveContext *
                                    const rs_context,
                                    HttpHandlerContext * const context)
{
    *rs_context = (ReplicationSlaveContext) {
        .context = context,
        .bev = NULL
    };
    return 0;
}

static ReplicationSlaveContext *
new_replication_context(HttpHandlerContext * const context)
{
    ReplicationSlaveContext *rs_context = malloc(sizeof *rs_context);
    if (rs_context == NULL) {
        return NULL;
    }
    init_replication_context(rs_context, context);
    
    return rs_context;
}

static void free_replication_context(ReplicationSlaveContext *
                                     const rs_context)
{
    if (rs_context == NULL) {
        return;
    }
    rs_context->context = NULL;
    if (rs_context->bev != NULL) {
        bufferevent_free(rs_context->bev);
    }
    free(rs_context);
}

typedef struct MemoryReadContext_ {
    unsigned char *pnt;
    size_t remaining;
} MemoryReadContext;

static ssize_t m_buffered_read(MemoryReadContext * const mrc,
                               void *dest, size_t size)
{
    if (size > mrc->remaining) {
        return (ssize_t) 0;
    }
    memcpy(dest, mrc->pnt, size);
    mrc->pnt += size;
    mrc->remaining -= size;
    
    return size;
}

static int m_replay_log_record(HttpHandlerContext * const context,
                               unsigned char *brp,
                               size_t sizeof_brp)
{
    char buf_cookie_head[sizeof DB_LOG_RECORD_COOKIE_HEAD - (size_t) 1U];
    char buf_cookie_tail[sizeof DB_LOG_RECORD_COOKIE_TAIL - (size_t) 1U];
    char buf_number[50];
    char *pnt;
    char *endptr;
    MemoryReadContext mrc_ = {
        .pnt = brp,
        .remaining = sizeof_brp
    };
    MemoryReadContext * const mrc = &mrc_;
    
    ssize_t readnb = m_buffered_read(mrc, buf_cookie_head,
                                     sizeof buf_cookie_head);
    if (readnb == (ssize_t) 0) {
        return 1;
    }
    if (readnb != (ssize_t) sizeof buf_cookie_head ||
        memcmp(buf_cookie_head, DB_LOG_RECORD_COOKIE_HEAD, readnb) != 0) {
        return -1;
    }
    pnt = buf_number;
    for (;;) {
        if (m_buffered_read(mrc, pnt, (size_t) 1U) != (ssize_t) 1U) {
            return -1;
        }
        if (*pnt == ' ') {
            break;
        }
        if (++pnt == &buf_number[sizeof buf_number]) {
            return -1;
        }
    }
    if (*buf_number == *DB_LOG_RECORD_COOKIE_MARK_CHAR) {
        time_t ts = (time_t) strtol(buf_number + 1U, &endptr, 16);
        if (endptr == NULL || endptr == buf_number + 1U || ts <= (time_t) 0) {
            return -1;
        }
        char t;
        if (m_buffered_read(mrc, &t, (size_t) 1U) != (size_t) 1U ||
            t != *DB_LOG_RECORD_COOKIE_TIMESTAMP_CHAR) {
            return -1;
        }
        readnb = m_buffered_read(mrc, buf_cookie_tail, sizeof buf_cookie_tail);
        if (readnb != (ssize_t) sizeof buf_cookie_tail ||
            memcmp(buf_cookie_tail, DB_LOG_RECORD_COOKIE_TAIL, readnb) != 0) {
            return -1;
        }
        return 0;
    }
    int verb = (int) strtol(buf_number, &endptr, 16);
    if (endptr == NULL || endptr == buf_number) {
        return -1;
    }
    
    pnt = buf_number;
    for (;;) {
        if (m_buffered_read(mrc, pnt, (size_t) 1U) != (ssize_t) 1U) {
            return -1;
        }
        if (*pnt == ':') {
            break;
        }
        if (++pnt == &buf_number[sizeof buf_number]) {
            return -1;
        }
    }
    size_t uri_len = (size_t) strtoul(buf_number, &endptr, 16);
    if (endptr == NULL || endptr == buf_number || uri_len <= (size_t) 0U) {
        return -1;
    }
    char *uri;
    if ((uri = malloc(uri_len + (size_t) 1U)) == NULL) {
        _exit(1);
    }    
    if (m_buffered_read(mrc, uri, uri_len) != (ssize_t) uri_len) {
        return -1;
    }
    *(uri + uri_len) = 0;
    
    pnt = buf_number;
    if (m_buffered_read(mrc, pnt, (size_t) 1U) != (ssize_t) 1U ||
        *pnt != ' ') {
        free(uri);
        return -1;
    }    
    for (;;) {
        if (m_buffered_read(mrc, pnt, (size_t) 1U) != (ssize_t) 1U) {
            free(uri);
            return -1;
        }
        if (*pnt == ':') {
            break;
        }
        if (++pnt == &buf_number[sizeof buf_number]) {
            free(uri);
            return -1;
        }
    }
    size_t body_len = (size_t) strtoul(buf_number, &endptr, 16);
    if (endptr == NULL || endptr == buf_number) {
        free(uri);
        return -1;
    }
    
    char *body = NULL;
    if (body_len > (size_t) 0U) {
        body = malloc(body_len + (size_t) 1U);
        if (body == NULL) {
            free(uri);
            _exit(1);
        }
    }
    if (body != NULL) {
        assert(body_len > (size_t) 0U);
        if (m_buffered_read(mrc, body, body_len) != (ssize_t) body_len) {
            free(uri);
            free(body);
            return -1;
        }
        *(body + body_len) = 0;
    }
    readnb = m_buffered_read(mrc, buf_cookie_tail, sizeof buf_cookie_tail);
    if (readnb != (ssize_t) sizeof buf_cookie_tail ||
        memcmp(buf_cookie_tail, DB_LOG_RECORD_COOKIE_TAIL, readnb) != 0) {
        free(body);
        free(uri);
        return -1;
    }
    fake_request(context, verb, uri, body, body_len, 0, 1);
    free(body);
    free(uri);
    
    return 0;
}

static void reader_writecb(struct bufferevent * const bev,
                           void * const context_)
{
    (void) bev;
    (void) context_;
}

static void reader_readcb(struct bufferevent * const bev,
                          void * const context_)
{
    HttpHandlerContext * const context = context_;
    struct evbuffer * const input = bufferevent_get_input(bev);
    struct evbuffer_ptr ptr;
    size_t len;
    unsigned char *buf = NULL;
    size_t sizeof_buf = (size_t) 0U;
    size_t line_size;

    while (evbuffer_get_length(input) != 0) {
        ptr = evbuffer_search_eol(input, NULL, &len, EVBUFFER_EOL_LF);
        if (ptr.pos == -1) {
            break;
        }
        line_size = (size_t) ptr.pos + (size_t) 1U;
        if (line_size > sizeof_buf) {
            assert(line_size > (size_t) 0U);
            unsigned char * const buf_ = realloc(buf, line_size);
            if (buf_ == NULL) {
                break;
            }
            buf = buf_;
            sizeof_buf = line_size;
        }
        assert(line_size <= sizeof_buf);
        evbuffer_remove(input, buf, line_size);
        m_replay_log_record(context, buf, line_size);
    }
    free(buf);
}

static void reader_eventcb(struct bufferevent * const bev,
                           const short what, void * const context_)
{
    HttpHandlerContext * const context = context_;

    if (what & BEV_EVENT_CONNECTED) {
        logfile(context, LOG_INFO, "Connected to master");
    } else if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (what & BEV_EVENT_EOF) {
            logfile(context, LOG_ERR, "Disconnected from master");
        } else {
            logfile_error(context, "Error while trying to connect to master");
        }
        logfile(context, LOG_ERR, "Aborting in 10 sec...");
        bufferevent_free(bev);
        stop_workers(context);
        sleep(10);
        _exit(1);
    }
}

int start_replication_slave(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port)
{
    struct evutil_addrinfo * ai;
    struct evutil_addrinfo hints;
    
    if (replication_master_ip == NULL || replication_master_port == NULL) {
        return 0;
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    const int gai_err = evutil_getaddrinfo(replication_master_ip,
                                           replication_master_port,
                                           &hints, &ai);
    if (gai_err != 0) {
        logfile(context, LOG_ERR,
                "Unable to resolve the replication slave address: [%s]",
                gai_strerror(gai_err));
        return -1;
    }
    struct bufferevent *bev;
    bev = bufferevent_socket_new(context->event_base, -1,
                                 BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    if (bev == NULL) {
        logfile_error(context, "Unable to create a socket to connect "
                      "in order to connect to a slave");
        evutil_freeaddrinfo(ai);
        return -1;
    }
    bufferevent_setcb(bev, reader_readcb, reader_writecb,
                      reader_eventcb, context);
    bufferevent_enable(bev, EV_READ);
    if (bufferevent_socket_connect(bev, ai->ai_addr, ai->ai_addrlen) != 0) {
        logfile_error(context, "Unable to connect to a slave");
        bufferevent_free(bev);
        evutil_freeaddrinfo(ai);
        return -1;        
    }
    evutil_freeaddrinfo(ai);
    ReplicationSlaveContext * const rs_context =
        new_replication_context(context);
    if (rs_context == NULL) {
        bufferevent_free(bev);        
        return -1;
    }
    rs_context->bev = bev;
    context->rs_context = rs_context;
    
    return 0;
}

void stop_replication_slave(HttpHandlerContext * const context)
{
    ReplicationSlaveContext * const rs_context = context->rs_context;
    if (rs_context != NULL) {
        free_replication_context(rs_context);
    }
    context->rs_context = NULL;
}
