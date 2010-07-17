
#ifndef __REPLICATION_H__
#define __REPLICATION_H__ 1

typedef struct ReplicationContext_ {
    HttpHandlerContext *context;
    unsigned int slaves_in_initial_download;
    struct evconnlistener *evl;
    Slab r_clients_slab;
} ReplicationContext;

typedef struct ReplicationClient_ {
    ReplicationContext *r_context;
    evutil_socket_t client_fd;
    int db_log_fd;
    struct evbuffer *evb;
} ReplicationClient;

int start_replication_server(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port);

void stop_replication_server(HttpHandlerContext * const context);

_Bool any_slave_in_initial_download(const HttpHandlerContext * const context);

#endif
