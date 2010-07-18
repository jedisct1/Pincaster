
#ifndef __REPLICATION_MASTER_H__
#define __REPLICATION_MASTER_H__ 1

typedef struct ReplicationMasterContext_ {
    HttpHandlerContext *context;
    unsigned int active_slaves;
    unsigned int slaves_in_initial_download;
    struct evconnlistener *evl;
    Slab r_clients_slab;
} ReplicationMasterContext;

typedef struct ReplicationClient_ {
    ReplicationMasterContext *rm_context;
    evutil_socket_t client_fd;
    int db_log_fd;
    struct bufferevent *bev;
    _Bool active;
} ReplicationClient;

int start_replication_master(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port);

void stop_replication_master(HttpHandlerContext * const context);

_Bool any_slave_in_initial_download(const HttpHandlerContext * const context);

int send_to_active_slaves(HttpHandlerContext * const context,
                          struct evbuffer * const r_entry_buffer);

#endif
