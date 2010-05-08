
#include "common.h"
#include "http_server.h"
#include "domain_system.h"

static int handle_special_op_system_rewrite(HttpHandlerContext * const context,
                                            SystemRewriteOp * const rewrite_op);

int handle_domain_system(struct evhttp_request * const req,
                         HttpHandlerContext * const context,
                         char *uri, char *opts, _Bool * const write_to_log,
                         const _Bool fake_req)
{
    (void) opts;
    (void) write_to_log;
    if (strcasecmp(uri, "ping") == 0) {
        Op op;
        SystemPingOp * const ping_op = &op.system_ping_op;
        
        *ping_op = (SystemPingOp) {
            .type = OP_TYPE_SYSTEM_PING,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid
        };
        pthread_mutex_lock(&context->mtx_cqueue);        
        if (push_cqueue(context->cqueue, ping_op) != 0) {
            pthread_mutex_unlock(&context->mtx_cqueue);
            return HTTP_SERVUNAVAIL;
        }
        pthread_mutex_unlock(&context->mtx_cqueue);
        pthread_cond_signal(&context->cond_cqueue);
        
        return 0;
    }
    if (req->type == EVHTTP_REQ_POST && strcasecmp(uri, "shutdown") == 0) {
        event_base_loopbreak(context->event_base);
        return 0;
    }
    if (req->type == EVHTTP_REQ_POST && strcasecmp(uri, "rewrite") == 0) {
        Op op;
        SystemRewriteOp * const rewrite_op = &op.system_rewrite_op;

        *rewrite_op = (SystemRewriteOp) {
            .type = OP_TYPE_SYSTEM_PING,
            .req = req,
            .fake_req = fake_req,
            .op_tid = ++context->op_tid            
        };
        handle_special_op_system_rewrite(context, rewrite_op);
        return 0;
    }
    return HTTP_NOTFOUND;
}

int handle_op_system_ping(SystemPingOp * const ping_op,
                          HttpHandlerContext * const context)
{
    yajl_gen json_gen;
    
    if (ping_op->fake_req != 0) {
        return 0;
    }
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    SystemPingOpReply * const ping_op_reply = &op_reply->system_ping_op_reply;
    
    *ping_op_reply = (SystemPingOpReply) {
        .type = OP_TYPE_SYSTEM_PING,
        .req = ping_op->req,
        .op_tid = ping_op->op_tid,
        .json_gen = NULL
    };
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    ping_op_reply->json_gen = json_gen;
    yajl_gen_string(json_gen,
                    (const unsigned char *) "pong",
                    (unsigned int) sizeof "pong" - (size_t) 1U);
    yajl_gen_string(json_gen,
                    (const unsigned char *) "pong",
                    (unsigned int) sizeof "pong" - (size_t) 1U);
    send_op_reply(context, op_reply);
    
    return 0;
}

char *get_tmp_log_file_name(void)
{
    DBLog * const db_log = &app_context.db_log;
    
    assert(db_log->db_log_file_name != NULL);
    const char *db_log_file_name = db_log->db_log_file_name;
    const size_t db_log_file_name_len = strlen(db_log_file_name);
    char *tmp_log_file_name = malloc(db_log_file_name_len +
                                     sizeof DB_LOG_TMP_SUFFIX);
    if (tmp_log_file_name == NULL) {
        return NULL;
    }
    memcpy(tmp_log_file_name, db_log_file_name, db_log_file_name_len);
    memcpy(tmp_log_file_name + db_log_file_name_len,
           DB_LOG_TMP_SUFFIX, sizeof DB_LOG_TMP_SUFFIX);
    
    return tmp_log_file_name;
}

typedef struct RebuildJournalLayerCBContext_ {
    HttpHandlerContext *context;
    int tmp_log_fd;
} RebuildJournalLayerCBContext;

static int write_log_buffer(struct evbuffer *log_buffer, const int log_fd)
{
    int ret = 0;
    
    do {
        size_t to_write = evbuffer_get_length(log_buffer);
        ssize_t written = evbuffer_write(log_buffer, log_fd);
        if (to_write > (size_t) 0U && written < (ssize_t) to_write) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep((useconds_t) 1000000 / 10);
                continue;
            }
            ret = -1;
        }
    } while (0);
 
    return ret;
}

static int rebuild_journal_layer_cb(void *context_, void *entry,
                                    const size_t sizeof_entry)
{
    (void) sizeof_entry;
    RebuildJournalLayerCBContext *context = context_;
    Layer * const layer = entry;
    int ret = 0;    
    assert(layer->name != NULL && *layer->name != 0);
    printf("Dumping layer: [%s] ...\n", layer->name);
    struct evbuffer *log_buffer;
    if ((log_buffer = evbuffer_new()) == NULL) {
        return -1;
    }
    evbuffer_add(log_buffer, DB_LOG_RECORD_COOKIE_HEAD,
                 sizeof DB_LOG_RECORD_COOKIE_HEAD - (size_t) 1U);
    const int verb = EVHTTP_REQ_POST;
    const size_t layer_name_len = strlen(layer->name);
    const size_t uri_len = context->context->encoded_base_uri_len +
        sizeof "layers/" - (size_t) 1U + layer_name_len +
        sizeof ".json" - (size_t) 1U;
    evbuffer_add_printf(log_buffer, "%x %zx:%slayers/%s.json %zx:",
                        verb, uri_len, context->context->encoded_base_uri,
                        layer->name, (size_t) 0U);
    evbuffer_add(log_buffer, DB_LOG_RECORD_COOKIE_TAIL,
                 sizeof DB_LOG_RECORD_COOKIE_TAIL - (size_t) 1U);
    ret = write_log_buffer(log_buffer, context->tmp_log_fd);
    evbuffer_free(log_buffer);
    
    return ret;
}


int rebuild_journal(HttpHandlerContext * const context, const int tmp_log_fd)
{
    RebuildJournalLayerCBContext cb_context = {
        .context = context,
        .tmp_log_fd = tmp_log_fd
    };
    slab_foreach(&context->layers_slab, rebuild_journal_layer_cb,
                 &cb_context);    
    return 0;
}

int rewrite_child(HttpHandlerContext * const context)
{
    (void) context;
    
    puts("Child");
    context->should_exit = 1;
    event_base_free(context->event_base);
    bufferevent_free(context->publisher_bev);
    bufferevent_free(context->consumer_bev);
    free_cqueue(context->cqueue);
    DBLog * const db_log = &app_context.db_log;
    if (db_log->db_log_fd != -1) {
        close(db_log->db_log_fd);
    }
    char *tmp_log_file_name = get_tmp_log_file_name();
    if (tmp_log_file_name == NULL) {
        return -1;
    }
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_EXLOCK
    flags |= O_EXLOCK;
#endif
#ifdef O_NOATIME
    flags |= O_NOATIME;
#endif
#ifdef O_LARGEFILE
    flags |= O_LARGEFILE;
#endif
    int db_log_fd = open(tmp_log_file_name, flags, (mode_t) 0600);
    free(tmp_log_file_name);
    if (db_log_fd == -1) {
        return -1;
    }
    rebuild_journal(context, db_log_fd);
    fsync(db_log_fd);
    close(db_log_fd);    
    
    return 0;
}

static int handle_special_op_system_rewrite(HttpHandlerContext * const context,
                                            SystemRewriteOp * const rewrite_op)
{
    if (rewrite_op->fake_req != 0) {
        return 0;
    }
    stop_workers(context);
    flush_db_log(1);
    pid_t child = fork();
    if (child == (pid_t) -1) {
        return HTTP_SERVUNAVAIL;
    }
    if (child == (pid_t) 0) {
        rewrite_child(context);
        _exit(0);
    }
    start_workers(context);

    yajl_gen json_gen;
    OpReply *op_reply = malloc(sizeof *op_reply);
    if (op_reply == NULL) {
        return HTTP_SERVUNAVAIL;
    }
    SystemRewriteOpReply * const rewrite_op_reply =
        &op_reply->system_rewrite_op_reply;
    
    *rewrite_op_reply = (SystemRewriteOpReply) {
        .type = OP_TYPE_SYSTEM_REWRITE,
        .req = rewrite_op->req,
        .op_tid = rewrite_op->op_tid,
        .json_gen = NULL
    };    
    if ((json_gen = new_json_gen(op_reply)) == NULL) {
        free(op_reply);
        return HTTP_SERVUNAVAIL;
    }
    rewrite_op_reply->json_gen = json_gen;
    yajl_gen_string(json_gen,
                    (const unsigned char *) "rewrite",
                    (unsigned int) sizeof "rewrite" - (size_t) 1U);
    yajl_gen_string(json_gen,
                    (const unsigned char *) "started",
                    (unsigned int) sizeof "started" - (size_t) 1U);
    send_op_reply(context, op_reply);

    return 0;
}

