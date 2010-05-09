
#include "common.h"
#include "http_server.h"
#include "query_parser.h"
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

static int flush_log_buffer(struct evbuffer *log_buffer, const int log_fd)
{
    int ret = 0;
    const size_t to_write = evbuffer_get_length(log_buffer);
    
    do {        
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

typedef struct RebuildJournalPropertyCBContext_ {
    struct evbuffer *body_buffer;
    _Bool first;
} RebuildJournalPropertyCBContext;

static int rebuild_journal_property_cb(void * context_,
                                       const void * const key,
                                       const size_t key_len,
                                       const void * const value,
                                       const size_t value_len)
{
    RebuildJournalPropertyCBContext * const context = context_;
    struct evbuffer * const body_buffer = context->body_buffer;
    
    if (context->first != 0) {
        context->first = 0;
    } else {
        evbuffer_add(body_buffer, "&", (size_t) 1U);
    }
    evbuffer_add(body_buffer, key, key_len);
    evbuffer_add(body_buffer, "=", (size_t) 1U);
    evbuffer_add(body_buffer, value, value_len);    
    
    return 0;
}

typedef struct RebuildJournalRecordCBContext_ {
    HttpHandlerContext * const http_handler_context;
    PanDB * const pan_db;
    struct evbuffer *log_buffer;
    int tmp_log_fd;
    const BinVal *encoded_layer_name;
} RebuildJournalRecordCBContext;

static int rebuild_journal_record_cb(void *context_, KeyNode * const key_node)
{
    RebuildJournalRecordCBContext * const context = context_;
    struct evbuffer * const log_buffer = context->log_buffer;
    evbuffer_add(log_buffer, DB_LOG_RECORD_COOKIE_HEAD,
                 sizeof DB_LOG_RECORD_COOKIE_HEAD - (size_t) 1U);
    const int verb = EVHTTP_REQ_PUT;
    const Key * const key = key_node->key;
    const size_t uri_len =
        context->http_handler_context->encoded_base_uri_len +
        sizeof "records/" - (size_t) 1U + context->encoded_layer_name->size +
        sizeof "/" - (size_t) 1U + key->len - (size_t) 1U +
        sizeof ".json" - (size_t) 1U;
    evbuffer_add_printf(log_buffer, "%x %zx:%srecords/%s/%s.json ",
                        verb, uri_len,
                        context->http_handler_context->encoded_base_uri,
                        context->encoded_layer_name->val, key->val);

    struct evbuffer * const body_buffer = evbuffer_new();
    if (body_buffer == NULL) {
        return -1;
    }
    RebuildJournalPropertyCBContext cb_context = {
        .body_buffer = body_buffer,
        .first = 1
    };
    slip_map_foreach(&key_node->properties, rebuild_journal_property_cb,
                     &cb_context);
    if (key_node->slot != NULL) {
        if (cb_context.first == 0) {
            evbuffer_add(body_buffer, "&", (size_t) 1U);
        }
#if PROJECTION
    const Position2D * const position = &key_node->slot->real_position;
#else
    const Position2D * const position = &key_node->slot->position;
#endif
        evbuffer_add_printf(body_buffer, "_loc=%f,%f",
                            (double) position->latitude,
                            (double) position->longitude);
    }
    evbuffer_add_printf(log_buffer, "%zx:", evbuffer_get_length(body_buffer));
    evbuffer_add_buffer(log_buffer, body_buffer);    
    evbuffer_free(body_buffer);
    evbuffer_add(log_buffer, DB_LOG_RECORD_COOKIE_TAIL,
                 sizeof DB_LOG_RECORD_COOKIE_TAIL - (size_t) 1U);
    if (evbuffer_get_length(log_buffer) > TMP_LOG_BUFFER_SIZE) {
        flush_log_buffer(log_buffer, context->tmp_log_fd);
    }    
    return 0;
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
    BinVal encoded_layer_name;
    init_binval(&encoded_layer_name);    
    BinVal decoded_layer_name;
    init_binval(&decoded_layer_name);
    decoded_layer_name.val = layer->name;
    decoded_layer_name.size = strlen(layer->name);    
    if (uri_encode_binval(&encoded_layer_name, &decoded_layer_name) != 0) {
        evbuffer_free(log_buffer);
        return -1;
    }
    const size_t uri_len = context->context->encoded_base_uri_len +
        sizeof "layers/" - (size_t) 1U + encoded_layer_name.size +
        sizeof ".json" - (size_t) 1U;
    evbuffer_add_printf(log_buffer, "%x %zx:%slayers/%s.json %zx:",
                        verb, uri_len, context->context->encoded_base_uri,
                        encoded_layer_name.val, (size_t) 0U);
    evbuffer_add(log_buffer, DB_LOG_RECORD_COOKIE_TAIL,
                 sizeof DB_LOG_RECORD_COOKIE_TAIL - (size_t) 1U);
    
    PanDB * const pan_db = &layer->pan_db;
    KeyNodes * const key_nodes = &pan_db->key_nodes;
    RebuildJournalRecordCBContext cb_context = {
        .http_handler_context = context->context,
        .pan_db = pan_db,
        .log_buffer = log_buffer,
        .tmp_log_fd = context->tmp_log_fd,
        .encoded_layer_name = &encoded_layer_name
    };
    if (key_nodes_foreach(key_nodes,
                          rebuild_journal_record_cb, &cb_context) != 0) {
        free_binval(&encoded_layer_name);
        evbuffer_free(log_buffer);        
        return -1;
    }
    assert(decoded_layer_name.max_size == (size_t) 0U);
    free_binval(&encoded_layer_name);
    ret = flush_log_buffer(log_buffer, context->tmp_log_fd);
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
    
    puts("Creating a new journal as a background process...");
    context->should_exit = 1;
    event_reinit(context->event_base);
    event_base_free(context->event_base);
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
    DBLog * const db_log = &app_context.db_log;
    if (rewrite_op->fake_req != 0 || db_log->db_log_file_name == NULL ||
        db_log->db_log_fd == -1) {
        return 0;
    }
    if (db_log->journal_is_being_rewritten != 0) {
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
    db_log->offset_before_fork = lseek(db_log->db_log_fd,
                                       (off_t) 0, SEEK_CUR);
    db_log->journal_is_being_rewritten = 1;
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

static int copy_data_between_fds(const int source_fd, const int target_fd)
{
    struct evbuffer *evbuf = evbuffer_new();
    int read_nb;
    
    while ((read_nb = evbuffer_read
            (evbuf, source_fd, TMP_LOG_BUFFER_SIZE)) > 0) {
        do {
            ssize_t written = evbuffer_write(evbuf, target_fd);
            if (written < read_nb) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep((useconds_t) 1000000 / 10);
                    continue;
                }
                evbuffer_free(evbuf);
                return -1;
            }
        } while(0);        
    }
    evbuffer_free(evbuf);
    
    return 0;
}

int system_rewrite_after_fork_cb(void)
{
    DBLog * const db_log = &app_context.db_log;
    db_log->journal_is_being_rewritten = 0;    
    char *tmp_log_file_name = get_tmp_log_file_name();
    if (tmp_log_file_name == NULL) {
        return -1;
    }
    puts("Completing the new journal with recent transactions...");
    assert(db_log->offset_before_fork != (off_t) -1);
    if (lseek(db_log->db_log_fd, db_log->offset_before_fork,
              SEEK_SET) == (off_t) -1) {
        free(tmp_log_file_name);        
        unlink(tmp_log_file_name);
        return -1;
    }
    int flags = O_RDWR | O_APPEND;
#ifdef O_EXLOCK
    flags |= O_EXLOCK;
#endif
#ifdef O_NOATIME
    flags |= O_NOATIME;
#endif
#ifdef O_LARGEFILE
    flags |= O_LARGEFILE;
#endif
    int tmp_log_fd = open(tmp_log_file_name, flags, (mode_t) 0600);
    if (tmp_log_fd == -1) {
        unlink(tmp_log_file_name);
    }
    if (copy_data_between_fds(db_log->db_log_fd, tmp_log_fd) != 0) {
        unlink(tmp_log_file_name);
        free(tmp_log_file_name);
        return -1;        
    }
    if (lseek(db_log->db_log_fd, (off_t) 0, SEEK_END) == -1) {
        exit(1);
    }
    fsync(tmp_log_fd);
    printf("Renaming [%s] to [%s]\n", tmp_log_file_name,
           db_log->db_log_file_name);
    if (rename(tmp_log_file_name, db_log->db_log_file_name) != 0) {
        perror("rename()");
        unlink(tmp_log_file_name);
        free(tmp_log_file_name);
        return -1;
    }
    free(tmp_log_file_name);
    if (close(db_log->db_log_fd) != 0) {
        perror("Unable to close the previous journal");
    }
    db_log->db_log_fd = tmp_log_fd;
    puts("Done - New journal activated");
    
    return 0;
}
