
#ifndef __COMMON_H__
#define __COMMON_H__ 1

#include <config.h>

#ifndef __GNUC__
# ifdef __attribute__
#  undef __attribute__
# endif
# define __attribute__(a)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>
#include "ext/queue.h"
#include "ext/tree.h"
#include <yajl_parse.h>
#include <yajl_gen.h>
#include <event2/thread.h>
#include <event.h>
#include <event2/listener.h>
#include "app_config.h"
#include "slab.h"
#include "cqueue.h"
#include "keys.h"
#include "stack.h"
#include "slipmap.h"
#include "pandb.h"
#include "key_nodes.h"
#include "utils.h"
#include "db_log.h"
#include "log.h"

#define INT_PROPERTY_COMMON_PREFIX     '_'
#define INT_PROPERTY_TYPE              "_type"
#define INT_PROPERTY_EXPIRES_AT        "_expires_at"
#define INT_PROPERTY_POSITION          "_loc"
#define INT_PROPERTY_DELETE_PREFIX     "_delete:"
#define INT_PROPERTY_DELETE_ALL_PREFIX "_delete_all"
#define INT_PROPERTY_ADD_INT_PREFIX    "_add_int:"
#define INT_PROPERTY_CONTENT           "$content"
#define INT_PROPERTY_CONTENT_TYPE      "$content_type"
#define INT_PROPERTY_LINK              "$link:"

typedef struct AppContext_ {
    char *server_ip;
    char *server_port;
    char *replication_master_ip;
    char *replication_master_port;
    char *replication_slave_ip;
    char *replication_slave_port;
    _Bool daemonize;
    char *log_file_name;
    int timeout;
    unsigned int nb_workers;
    size_t max_queued_replies;
    LayerType default_layer_type;
    Accuracy default_accuracy;
    size_t bucket_size;
    Dimension dimension_accuracy;
    DBLog db_log;
    struct HttpHandlerContext_ *http_handler_context;
} AppContext;

#ifndef DEFINE_GLOBALS
extern AppContext app_context;
#else
AppContext app_context;
#endif

#ifndef AUTOMATICALLY_CREATE_LAYERS
# define AUTOMATICALLY_CREATE_LAYERS 0
#endif

#ifdef __APPLE_CC__
int fdatasync(int fd);
#endif

#endif
