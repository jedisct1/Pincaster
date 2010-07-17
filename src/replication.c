
#include "common.h"
#include "http_server.h"
#include "replication.h"

#define REPLICATION_LISTEN_BACKLOG 128

int init_replication_context(ReplicationContext * const r_context,
                             HttpHandlerContext * const context)
{
    r_context->context = context;    
    r_context->slaves_in_initial_download = 0U;
    
    return 0;
}

ReplicationContext *new_replication_context(HttpHandlerContext * const context) {
    ReplicationContext *r_context = malloc(sizeof *r_context);
    if (r_context == NULL) {
        return NULL;
    }
    init_replication_context(r_context, context);
    
    return r_context;
}

void free_replication_context(ReplicationContext * const r_context)
{
    r_context->context = NULL;
    free(r_context);
}

int init_replication_client(ReplicationClient * const r_client,
                            ReplicationContext * const r_context)
{
    *r_client = (ReplicationClient) {
        .r_context = r_context,
        .client_fd = -1,
        .db_log_fd = -1
    };
    return 0;
}

ReplicationClient *new_replication_client(ReplicationContext * const r_context)
{
    ReplicationClient *r_client = malloc(sizeof *r_client);
    if (r_client == NULL) {
        return NULL;
    }
    init_replication_client(r_client, r_context);
    
    return r_client;    
}

void free_replication_client(ReplicationClient * const r_client)
{
    if ((r_client->client_fd) != -1) {
        evutil_closesocket(r_client->client_fd);
        r_client->client_fd = 1;
    }
    if ((r_client->db_log_fd) != -1) {
        close(r_client->db_log_fd);
        r_client->db_log_fd = 1;
    }
    r_client->r_context = NULL;
    free(r_client);
}

static void sender_writecb(struct bufferevent * const bev,
                           void * const r_client_)
{
    ReplicationClient * const r_client = r_client_;
    ReplicationContext * const r_context = r_client->r_context;
    
	if (evbuffer_get_length(bufferevent_get_output(bev)) != 0) {
        return;
    }
    bufferevent_disable(bev,EV_READ | EV_WRITE);
    bufferevent_free(bev);
    assert(r_context->slaves_in_initial_download > 0U);
    r_context->slaves_in_initial_download--;
}

static void sender_readcb(struct bufferevent * const bev,
                          void * const r_client_)
{
    (void) bev;
    (void) r_client_;
    
    puts("ReadCB");
}

static void sender_errorcb(struct bufferevent * const bev,
                           const short what, void * const r_client_)
{    
    ReplicationClient * const r_client = r_client_;
    ReplicationContext * const r_context = r_client->r_context;
    HttpHandlerContext * const context = r_context->context;
    
    logfile(context, LOG_NOTICE, "Slave network error [%d]", what);
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_free(bev);
    assert(r_context->slaves_in_initial_download > 0U);    
    r_context->slaves_in_initial_download--;
}

static void acceptcb(struct evconnlistener * const listener,
                     evutil_socket_t client_fd,
                     struct sockaddr * const addr, int socklen,
                     void * const context_)
{
    HttpHandlerContext * const context = context_;
    ReplicationContext * const r_context = context->r_context;
    ReplicationClient *r_client;

    (void) listener;
    (void) addr;
    (void) socklen;
    assert(r_context != NULL);
    assert(app_context.db_log.db_log_file_name != NULL);
    logfile(context, LOG_NOTICE, "New slave connected");
    if (r_context->slaves_in_initial_download == UINT_MAX) {
        logfile(context, LOG_WARNING, "Too many active slaves");
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
    if ((r_client = new_replication_client(r_context)) == NULL) {
        logfile_error(context, "Unable to allocate a replication client");
        evutil_closesocket(client_fd);
        return;        
    }
    r_client->client_fd = client_fd;
    bufferevent_setcb(bev, sender_readcb, sender_writecb,
                      sender_errorcb, context);
    const char s[] = "EXPORTED JOURNAL 1\n";
    bufferevent_write(bev, s, sizeof s);
    struct evbuffer *evb = bufferevent_get_output(bev);
    if ((r_client->db_log_fd = open
         (app_context.db_log.db_log_file_name, O_RDONLY)) == -1) {
        logfile_error(context, "Unable to allocate a replication client");
        free_replication_client(r_client);
        return;
    }
    struct stat st;
    if (fstat(r_client->db_log_fd, &st) != 0) {
        logfile_error(context, "Unable to retrieve the journal file size");
        free_replication_client(r_client);
        return;
    }
    if (st.st_size == (off_t) 0) {
        logfile(context, LOG_NOTICE, "Slave is asking for an empty journal");
    }
    evbuffer_add_file(evb, r_client->db_log_fd, (off_t) 0,
                      (off_t) st.st_size);
    r_context->slaves_in_initial_download++;
    bufferevent_enable(bev, EV_WRITE);
}

int start_replication_server(HttpHandlerContext * const context,
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
    if (evutil_getaddrinfo(replication_master_ip,
                           replication_master_port,
                           NULL, &ai) != 0) {
        logfile_error(context, "Unable to start the replication service");
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
                 
    return 0;
}

void stop_replication_server(HttpHandlerContext * const context)
{
    ReplicationContext * const r_context = context->r_context;
    if (r_context == NULL) {
        return;
    }
    free_replication_context(r_context);
}
