
#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__ 1

#include <evhttp.h>

#ifndef ENCODED_API_BASE_URI
# define ENCODED_API_BASE_URI "/api/1.0/"
#endif
#ifndef ENCODED_PUBLIC_BASE_URI
# define ENCODED_PUBLIC_BASE_URI "/public/"
#endif
#ifndef SERVER_NAME 
# define SERVER_NAME PACKAGE_STRING
#endif
#ifndef MAX_URI_LEN
# define MAX_URI_LEN (size_t) 10000U
#endif
#ifndef BEAUTIFY_JSON
# ifdef DEBUG
#  define BEAUTIFY_JSON 1
# else
#  define BEAUTIFY_JSON 0
# endif
#endif

typedef enum OpType_ {
    OP_TYPE_NONE,
    OP_TYPE_ERROR,
        
    OP_TYPE_SYSTEM_PING,
    OP_TYPE_SYSTEM_REWRITE,        
        
    OP_TYPE_LAYERS_CREATE,
    OP_TYPE_LAYERS_DELETE,
    OP_TYPE_LAYERS_INDEX,
        
    OP_TYPE_RECORDS_PUT,
    OP_TYPE_RECORDS_GET,
    OP_TYPE_RECORDS_DELETE,        
        
    OP_TYPE_SEARCH_NEARBY,
    OP_TYPE_SEARCH_IN_RECT,
    OP_TYPE_SEARCH_IN_KEYS,
} OpType;

typedef uint_fast64_t OpTID;

typedef struct BareOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;
    OpTID op_tid;
} BareOp;

typedef struct SystemPingOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;    
} SystemPingOp;

typedef struct SystemRewriteOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;    
} SystemRewriteOp;

typedef struct LayersCreateOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
} LayersCreateOp;

typedef struct LayersDeleteOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
} LayersDeleteOp;

typedef struct LayersIndexOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
} LayersIndexOp;

typedef struct RecordsPutOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;    
    Key *key;    
    Position2D position;
    SlipMap *properties;
    SlipMap *special_properties;    
    _Bool position_set;
    time_t expires_at;
} RecordsPutOp;

typedef struct RecordsGetOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
    Key *key;
    _Bool with_links;
} RecordsGetOp;

typedef struct RecordsDeleteOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
    Key *key;    
} RecordsDeleteOp;

typedef struct SearchNearbyOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;    
    Position2D position;
    Dimension radius;
    SubSlots limit;
    Dimension epsilon;
    _Bool with_properties;
} SearchNearbyOp;

typedef struct SearchInRectOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
    Rectangle2D rect;
    SubSlots limit;
    Dimension epsilon;
    _Bool with_properties;
} SearchInRectOp;

typedef struct SearchInKeysOp_ {
    OpType type;
    struct evhttp_request *req;
    _Bool fake_req;    
    OpTID op_tid;
    Key *layer_name;
    Key *pattern;
    SubSlots limit;
    _Bool with_properties;
    _Bool with_content;
} SearchInKeysOp;

typedef union Op_ {
    BareOp          bare_op;
    SystemPingOp    system_ping_op;
    SystemRewriteOp system_rewrite_op;    
    LayersCreateOp  layers_create_op;
    LayersDeleteOp  layers_delete_op;
    LayersIndexOp   layers_index_op;
    RecordsPutOp    records_put_op;
    RecordsGetOp    records_get_op;
    RecordsDeleteOp records_delete_op;    
    SearchNearbyOp  search_nearby_op;
    SearchInRectOp  search_in_rect_op;
    SearchInKeysOp  search_in_keys_op;
} Op;

typedef struct BareOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} BareOpReply;

typedef struct ErrorOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} ErrorOpReply;

typedef struct SystemPingOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} SystemPingOpReply;

typedef struct SystemRewriteOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} SystemRewriteOpReply;

typedef struct LayersCreateOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} LayersCreateOpReply;

typedef struct LayersDeleteOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} LayersDeleteOpReply;

typedef struct LayersIndexOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} LayersIndexOpReply;

typedef struct RecordsPutOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} RecordsPutOpReply;

typedef struct RecordsGetOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} RecordsGetOpReply;

typedef struct RecordsDeleteOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} RecordsDeleteOpReply;

typedef struct SearchNearbyOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} SearchNearbyOpReply;

typedef struct SearchInRectOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} SearchInRectOpReply;

typedef struct SearchInKeysOpReply_ {
    OpType type;
    struct evhttp_request *req;
    OpTID op_tid;
    yajl_gen json_gen;
} SearchInKeysOpReply;

typedef union OpReply_ {
    BareOpReply          bare_op_reply;
    ErrorOpReply         error_op_reply;    
    SystemPingOpReply    system_ping_op_reply;
    SystemRewriteOpReply system_rewrite_op_reply;    
    LayersCreateOpReply  layers_create_op_reply;
    LayersDeleteOpReply  layers_delete_op_reply;
    LayersIndexOpReply   layers_index_op_reply;
    RecordsPutOpReply    records_put_op_reply;
    RecordsGetOpReply    records_get_op_reply;
    RecordsDeleteOpReply records_delete_op_reply;    
    SearchNearbyOpReply  search_nearby_op_reply;
    SearchInRectOpReply  search_in_rect_op_reply;
    SearchInKeysOpReply  search_in_keys_op_reply;    
} OpReply;

typedef struct Layer_ {
    char *name;
    PanDB pan_db;
} Layer;

typedef struct HttpHandlerContext_ {
    sig_atomic_t should_exit;
    pthread_t *thr_workers;
    struct event_base *event_base;
    OpTID op_tid;
    const char *encoded_api_base_uri;
    size_t encoded_api_base_uri_len;
    const char *encoded_public_base_uri;
    size_t encoded_public_base_uri_len;
    CQueue *cqueue;
    pthread_mutex_t mtx_cqueue;
    pthread_cond_t cond_cqueue;
    pthread_rwlock_t rwlock_layers;
    struct bufferevent *publisher_bev;
    struct bufferevent *consumer_bev;
    Slab layers_slab;
    size_t nb_layers;
    struct event ev_flush_log_db;
    struct event ev_expiration_cron;
    Slab expirables_slab;
    time_t now;
    char *log_file_name;
    int log_fd;
} HttpHandlerContext;

typedef int (*DomainHandler)(struct evhttp_request * const req,
                             HttpHandlerContext * const context,
                             char *uri, char *opts,
                             _Bool * const write_to_log,
                             const _Bool fake_req);

typedef struct Domain_ {
    const char *uri_part;
    DomainHandler domain_handler;
} Domain;

int http_server(void);

yajl_gen new_json_gen(const OpReply * const op_reply);

int send_op_reply(HttpHandlerContext * const context,
                  const OpReply * const op_reply);

int fake_request(HttpHandlerContext * const context,
                 const int verb, char * const uri,
                 const char *body, const size_t body_len);

int replay_log(HttpHandlerContext * const context);

int start_workers(HttpHandlerContext * const context);
int stop_workers(HttpHandlerContext * const context);

#endif
