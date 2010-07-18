
#include "common.h"
#include "http_server.h"
#include "replication_slave.h"

int start_replication_slave(HttpHandlerContext * const context,
                             const char * const replication_master_ip,
                             const char * const replication_master_port)
{
    (void) context;
    (void) replication_master_ip;
    (void) replication_master_port;
    
    return -1;
}

void stop_replication_slave(HttpHandlerContext * const context)
{
    (void) context;
}
