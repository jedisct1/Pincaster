
#ifndef __COMMON_H__
#define __COMMON_H__ 1

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include "../src/yajl/api/yajl_parse.h"
#include "../src/yajl/api/yajl_gen.h"
#include "ext/queue.h"
#include "ext/tree.h"
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

typedef struct AppContext_ {
    char *server_ip;
    char *server_port;
    int timeout;
    unsigned int nb_workers;
    size_t max_queued_replies;
    LayerType default_layer_type;
    Accuracy default_accuracy;
    size_t bucket_size;
    Dimension dimension_accuracy;
    DBLog db_log;
} AppContext;

#ifdef DEFINE_GLOBALS
extern AppContext app_context;
#else
AppContext app_context;
#endif

typedef struct BinVal_ {
    char *val;
    size_t size;
    size_t max_size;
} BinVal;

#ifndef AUTOMATICALLY_CREATE_LAYERS
# define AUTOMATICALLY_CREATE_LAYERS 0
#endif

#ifdef __APPLE_CC__
int fdatasync(int fd);
#endif

#endif
