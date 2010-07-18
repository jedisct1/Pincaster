
#include "common.h"
#include "http_server.h"
#include "replication_master.h"

#define REPLICATION_LISTEN_BACKLOG 128
#define REPLICATION_MAX_LAG 256U * 1024U * 1024U

static void free_replication_client(ReplicationClient * const r_client,
                                    const _Bool remove_from_slab);

static int init_replication_context(ReplicationMasterContext *
                                    const rm_context,
                                    HttpHandlerContext * const context)
{
    *rm_context = (ReplicationMasterContext) {
        .context = context,
        .active_slaves = 0U,
        .slaves_in_initial_download = 0U,
        .evl = NULL
    };
    init_slab(&rm_context->r_clients_slab, sizeof(ReplicationClient),
              "replication clients slab");
    return 0;
}

static ReplicationMasterContext *
new_replication_context(HttpHandlerContext * const context)
{
    ReplicationMasterContext *rm_context = malloc(sizeof *rm_context);
    if (rm_context == NULL) {
        return NULL;
    }
    init_replication_context(rm_context, context);
    
    return rm_context;
}

static void free_replication_context_cb(void * const r_client_)
{
    free_replication_client(r_client_, 0);
}

static void free_replication_context(ReplicationMasterContext *
                                     const rm_context)
{
    if (rm_context == NULL) {
        return;
    }
    rm_context->context = NULL;
    if (rm_context->evl != NULL) {
        evconnlistener_free(rm_context->evl);
    }
    free_slab(&rm_context->r_clients_slab, free_replication_context_cb);
    free(rm_context);
}

static int init_replication_client(ReplicationClient * const r_client,
                                   ReplicationMasterContext * const rm_context)
{
    *r_client = (ReplicationClient) {
        .rm_context = rm_context,
        .client_fd = -1,
        .db_log_fd = -1,
        .bev = NULL,
        .active = 0
    };
    return 0;
}

static ReplicationClient *
new_replication_client(ReplicationMasterContext * const rm_context)
{
    ReplicationClient r_client_;
    ReplicationClient *r_client;
        
    init_replication_client(&r_client_, rm_context);
    r_client = add_entry_to_slab(&rm_context->r_clients_slab, &r_client_);
    if (r_client == NULL) {
        free_replication_client(&r_client_, 1);
    }
    return r_client;
}

static void free_replication_client(ReplicationClient * const r_client,
                                    const _Bool remove_from_slab)
{
    if (r_client == NULL) {
        return;
    }
    if (r_client->bev != NULL) {
        bufferevent_free(r_client->bev);
        r_client->bev = NULL;
    }
    if (r_client->client_fd != -1) {
        evutil_closesocket(r_client->client_fd);
        r_client->client_fd = -1;
    }
    if ((r_client->db_log_fd) != -1) {
        close(r_client->db_log_fd);
        r_client->db_log_fd = -1;
    }
    ReplicationMasterContext * const rm_context = r_client->rm_context;
    r_client->rm_context = NULL;
    if (remove_from_slab != 0) {
        remove_entry_from_slab(&rm_context->r_clients_slab, r_client);
    }
}

static void log_activity(const ReplicationMasterContext * const rm_context,
                         const char * const msg)
{
    logfile(rm_context->context, LOG_INFO,
            "%s - "
            "[%u] slave%s downloading the initial journal and "
            "[%u] active slave%s",
            msg,
            rm_context->slaves_in_initial_download,
            rm_context->slaves_in_initial_download != 1 ? "s" : "",
            rm_context->active_slaves,
            rm_context->active_slaves != 1 ? "s" : "");
}

static void sender_writecb(struct bufferevent * const bev,
                           void * const r_client_)
{
    ReplicationClient * const r_client = r_client_;
    ReplicationMasterContext * const rm_context = r_client->rm_context;

	if (evbuffer_get_length(bufferevent_get_output(bev)) != 0) {
        return;
    }
    if (r_client->active == 1) {
        assert(rm_context->active_slaves > 0U);
    }
}

static void sender_readcb(struct bufferevent * const bev,
                          void * const r_client_)
{
    (void) bev;
    (void) r_client_;
}

static void sender_eventcb(struct bufferevent * const bev,
                           const short what, void * const r_client_)
{
    ReplicationClient * const r_client = r_client_;
    ReplicationMasterContext * const rm_context = r_client->rm_context;

    if (what & BEV_EVENT_ERROR) {
        logfile(rm_context->context, LOG_WARNING, "Slave network error: [%s]",
                strerror(errno));
    }
    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_disable(bev, EV_READ | EV_WRITE);
        if (r_client->active == 0) {
            assert(rm_context->slaves_in_initial_download > 0U);
            rm_context->slaves_in_initial_download--;
        } else if (r_client->active != 0) {
            assert(rm_context->active_slaves > 0U);
            rm_context->active_slaves--;
        }
        if (what & BEV_EVENT_EOF) {
            log_activity(rm_context, "Slave disconnected");
        }
        free_replication_client(r_client, 1);
    }
}

static void acceptcb(struct evconnlistener * const listener,
                     evutil_socket_t client_fd,
                     struct sockaddr * const addr, int socklen,
                     void * const context_)
{
    HttpHandlerContext * const context = context_;
    ReplicationMasterContext * const rm_context = context->rm_context;
    ReplicationClient *r_client;

    (void) listener;
    (void) addr;
    (void) socklen;
    assert(rm_context != NULL);
    assert(app_context.db_log.db_log_file_name != NULL);
    logfile_noformat(context, LOG_INFO, "New slave connected");
    if (rm_context->slaves_in_initial_download == UINT_MAX) {
        logfile(context, LOG_WARNING, "Too many downloading slaves");
        evutil_closesocket(client_fd);        
        return;
    }
    struct bufferevent *bev;
    bev = bufferevent_socket_new(context->event_base, client_fd,
                                 BEV_OPT_CLOSE_ON_FREE);
    if (bev == NULL) {
        logfile_error(context, "Unable to allocate a bufferevent");
        evutil_closesocket(client_fd);
        return;
    }    
    if ((r_client = new_replication_client(rm_context)) == NULL) {
        logfile_error(context, "Unable to allocate a replication client");
        bufferevent_free(bev);
        evutil_closesocket(client_fd);
        return;        
    }
    r_client->bev = bev;
    r_client->client_fd = client_fd;
    bufferevent_setcb(bev, sender_readcb, sender_writecb,
                      sender_eventcb, r_client);
    const char s[] = "EXPORTED JOURNAL 1\n";
    bufferevent_write(bev, s, sizeof s);
    struct evbuffer *evb = bufferevent_get_output(bev);
    if ((r_client->db_log_fd = open
         (app_context.db_log.db_log_file_name, O_RDONLY)) == -1) {
        logfile_error(context, "Unable to allocate a replication client");
        free_replication_client(r_client, 1);
        return;
    }
    struct stat st;
    if (fstat(r_client->db_log_fd, &st) != 0) {
        logfile_error(context, "Unable to retrieve the journal file size");
        free_replication_client(r_client, 1);
        return;
    }
    if (st.st_size == (off_t) 0) {        
        logfile(context, LOG_NOTICE, "Slave is asking for an empty journal");
    }
    evbuffer_add_file(evb, r_client->db_log_fd, (off_t) 0,
                      (off_t) st.st_size);
    r_client->db_log_fd = -1;
    rm_context->slaves_in_initial_download++;

    bufferevent_enable(bev, EV_WRITE);
    log_activity(rm_context, "New slave registered");
    
    assert(rm_context->slaves_in_initial_download > 0U);
    rm_context->slaves_in_initial_download--;
    assert(r_client->active == 0);
    r_client->active = 1;
    rm_context->active_slaves++;
    log_activity(rm_context, "Initial journal sent to slave");    
}

int start_replication_master(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port)
{
    struct evutil_addrinfo * ai;
    struct evutil_addrinfo hints;

    if (app_context.db_log.db_log_file_name == NULL ||
        app_context.db_log.db_log_fd == -1) {
        logfile(context, LOG_WARNING, "Replication requires a journal");
        return -1;
    }
    if (replication_master_ip == NULL || replication_master_port == NULL) {
        return 0;
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = EVUTIL_AI_PASSIVE | EVUTIL_AI_ADDRCONFIG;
    const int gai_err = evutil_getaddrinfo(replication_master_ip,
                                           replication_master_port,
                                           NULL, &ai);
    if (gai_err != 0) {
        logfile(context, LOG_ERR,
                "Unable to start the replication service: [%s]",
                gai_strerror(gai_err));
        return -1;
    }
    struct evconnlistener *evl;
    evl = evconnlistener_new_bind
        (context->event_base,
            acceptcb, context,
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_EXEC,
            REPLICATION_LISTEN_BACKLOG, ai->ai_addr, ai->ai_addrlen);
    if (evl == NULL) {
        logfile_error(context, "Unable to bind the replication service");
        evutil_freeaddrinfo(ai);        
        return -1;
    }
    evutil_freeaddrinfo(ai);
    ReplicationMasterContext * const rm_context =
        new_replication_context(context);
    if (rm_context == NULL) {
        evconnlistener_free(evl);
        return -1;
    }
    rm_context->evl = evl;
    context->rm_context = rm_context;
    
    return 0;
}

void stop_replication_master(HttpHandlerContext * const context)
{
    ReplicationMasterContext * const rm_context = context->rm_context;
    if (rm_context != NULL) {
        free_replication_context(rm_context);
    }
    context->rm_context = NULL;
}

_Bool any_slave_in_initial_download(const HttpHandlerContext * const context)
{
    const ReplicationMasterContext * const rm_context = context->rm_context;
    
    if (rm_context == NULL) {
        return 0;
    }
    return rm_context->slaves_in_initial_download > 0U;
}

typedef struct SendToActiveSlavesContextCB_ {
    unsigned char *r_entry;
    size_t sizeof_r_entry;
} SendToActiveSlavesContextCB;

static int send_to_active_slaves_cb(void * const context_cb_,
                                    void * const r_client_,
                                    const size_t sizeof_entry)
{
    SendToActiveSlavesContextCB *context_cb = context_cb_;
    ReplicationClient * const r_client = r_client_;

    (void) sizeof_entry;
    if (r_client->active == 0 || r_client->bev == NULL ||
        r_client->client_fd == -1) {
        return 0;
    }
    struct evbuffer * const evb = bufferevent_get_output(r_client->bev);    
    if (evbuffer_get_length(evb) +
        context_cb->sizeof_r_entry > REPLICATION_MAX_LAG) {
        ReplicationMasterContext * const rm_context = r_client->rm_context;
        if (r_client->active == 0) {
            assert(rm_context->slaves_in_initial_download > 0U);
            rm_context->slaves_in_initial_download--;
        } else if (r_client->active != 0) {
            assert(rm_context->active_slaves > 0U);
            rm_context->active_slaves--;
        }
        log_activity(rm_context, "Slave is lagging way too much - "
                     "disconnecting");
        bufferevent_free(r_client->bev);
        r_client->bev = NULL;
        evutil_closesocket(r_client->client_fd);
        r_client->client_fd = -1;
        /* XXX - Remove entry from slab */
        
        return 0;
    }
    evbuffer_add(evb, context_cb->r_entry, context_cb->sizeof_r_entry);
    
    return 0;    
}

int send_to_active_slaves(HttpHandlerContext * const context,
                          struct evbuffer * const r_entry_buffer)
{
    SendToActiveSlavesContextCB context_cb;
    
    context_cb.sizeof_r_entry = evbuffer_get_length(r_entry_buffer);
    context_cb.r_entry = evbuffer_pullup(r_entry_buffer, (ev_ssize_t) -1);
    
    slab_foreach(&context->rm_context->r_clients_slab,
                 send_to_active_slaves_cb, &context_cb);
    return 0;
}
