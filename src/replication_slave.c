
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

static void reader_writecb(struct bufferevent * const bev,
                           void * const context_)
{
    
}

static void reader_readcb(struct bufferevent * const bev,
                          void * const context_)
{

}

static void reader_eventcb(struct bufferevent * const bev,
                           const short what, void * const context_)
{

}

int start_replication_slave(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port)
{
    struct evutil_addrinfo * ai;
    struct evutil_addrinfo hints;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    const int gai_err = evutil_getaddrinfo(replication_master_ip,
                                           replication_master_port,
                                           NULL, &ai);
    if (gai_err != 0) {
        logfile(context, LOG_ERR,
                "Unable to resolve the replication slave address: [%s]",
                gai_strerror(gai_err));
        return -1;
    }
    struct bufferevent *bev;
    bev = bufferevent_socket_new(context->event_base, -1,
                                 BEV_OPT_CLOSE_ON_FREE);
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
    
    return -0;
}

void stop_replication_slave(HttpHandlerContext * const context)
{
    ReplicationSlaveContext * const rs_context = context->rs_context;
    if (rs_context != NULL) {
        free_replication_context(rs_context);
    }
    context->rs_context = NULL;
}
