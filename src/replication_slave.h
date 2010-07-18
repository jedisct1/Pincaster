
#ifndef __REPLICATION_SLAVE_H__
#define __REPLICATION_SLAVE_H__ 1

typedef struct ReplicationSlaveContext_ {
    HttpHandlerContext *context;
    struct bufferevent *bev;
} ReplicationSlaveContext;

int start_replication_slave(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port);

void stop_replication_slave(HttpHandlerContext * const context);

#endif

