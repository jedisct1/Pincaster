
#include "common.h"
#include "http_server.h"
#include "domain_system.h"
#include "domain_layers.h"
#include "domain_records.h"
#include "domain_search.h"
#include "handle_consumer_ops.h"
#include "expirables.h"
#include "db_log.h"

static struct event_base *event_base;

static void http_close_cb(struct evhttp_connection * const cnx,
                          void * const context_)
{
    (void) context_;
    evhttp_connection_set_closecb(cnx, NULL, NULL);
}

int send_op_reply(HttpHandlerContext * const context,
                  const OpReply * const op_reply)
{
    pthread_mutex_t fake_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t fake_cond = PTHREAD_COND_INITIALIZER;
    struct timeval tv;
    struct timespec ts;
    
    while (evbuffer_get_length(context->publisher_bev->output) >=
           app_context.max_queued_replies * sizeof op_reply ||
           bufferevent_write(context->publisher_bev, &op_reply,
                             sizeof op_reply) != 0) {        
        if (gettimeofday(&tv, NULL) != 0) {
            break;
        }
        ts = (struct timespec) {
            .tv_sec = tv.tv_sec + 1L,
            .tv_nsec = tv.tv_usec * 1000L
        };
        pthread_mutex_lock(&fake_mutex);
        pthread_cond_timedwait(&fake_cond, &fake_mutex, &ts);
        pthread_mutex_unlock(&fake_mutex);
    }
    bufferevent_flush(context->publisher_bev, EV_WRITE, BEV_FLUSH);
    
    return 0;
}

yajl_gen new_json_gen(const OpReply * const op_reply)
{
    yajl_gen json_gen = yajl_gen_alloc(&(yajl_gen_config) {
        .beautify = BEAUTIFY_JSON,
        .indentString = "\t"
    }, NULL);
    if (json_gen == NULL) {
        return NULL;
    }
    yajl_gen_map_open(json_gen);
    yajl_gen_string(json_gen, (const unsigned char *) "tid",
                    sizeof "tid" - (size_t) 1U);
    char buf[sizeof "18446744073709551616"];
    snprintf(buf, sizeof buf, "%" PRIdFAST64, op_reply->bare_op_reply.op_tid);
    yajl_gen_number(json_gen, buf, strlen(buf));
    
    return json_gen;
}

static int worker_do_work(HttpHandlerContext * const context)
{
    Op *op_;
    Op op;
    
    pthread_mutex_lock(&context->mtx_cqueue);
stab:
    if (context->should_exit != 0) {
        pthread_mutex_unlock(&context->mtx_cqueue);        
        return 1;
    }
    op_ = shift_cqueue(context->cqueue);
    if (op_ != NULL && op_->bare_op.req != NULL) {
        int ret = -1;
        op = *op_;
        pthread_mutex_unlock(&context->mtx_cqueue);
        if (op.bare_op.type == OP_TYPE_SYSTEM_PING) {
            ret = handle_op_system_ping(&op.system_ping_op, context);
        } else if (op.bare_op.type == OP_TYPE_LAYERS_CREATE) {
            pthread_rwlock_wrlock(&context->rwlock_layers);
            ret = handle_op_layers_create(&op.layers_create_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_LAYERS_DELETE) {
            pthread_rwlock_wrlock(&context->rwlock_layers);
            ret = handle_op_layers_delete(&op.layers_delete_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_LAYERS_INDEX) {
            pthread_rwlock_rdlock(&context->rwlock_layers);
            ret = handle_op_layers_index(&op.layers_index_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_RECORDS_PUT) {
            pthread_rwlock_wrlock(&context->rwlock_layers);
            ret = handle_op_records_put(&op.records_put_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_RECORDS_GET) {
#if AUTOMATICALLY_CREATE_LAYERS
            pthread_rwlock_wrlock(&context->rwlock_layers);
#else
            pthread_rwlock_rdlock(&context->rwlock_layers);
#endif
            ret = handle_op_records_get(&op.records_get_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_RECORDS_DELETE) {
            pthread_rwlock_wrlock(&context->rwlock_layers);
            ret = handle_op_records_delete(&op.records_delete_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_SEARCH_NEARBY) {
#if AUTOMATICALLY_CREATE_LAYERS
            pthread_rwlock_wrlock(&context->rwlock_layers);
#else
            pthread_rwlock_rdlock(&context->rwlock_layers);
#endif
            ret = handle_op_search_nearby(&op.search_nearby_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_SEARCH_IN_RECT) {
#if AUTOMATICALLY_CREATE_LAYERS
            pthread_rwlock_wrlock(&context->rwlock_layers);
#else
            pthread_rwlock_rdlock(&context->rwlock_layers);
#endif
            ret = handle_op_search_in_rect(&op.search_in_rect_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else if (op.bare_op.type == OP_TYPE_SEARCH_IN_KEYS) {
#if AUTOMATICALLY_CREATE_LAYERS
            pthread_rwlock_wrlock(&context->rwlock_layers);
#else
            pthread_rwlock_rdlock(&context->rwlock_layers);
#endif
            ret = handle_op_search_in_keys(&op.search_in_keys_op, context);
            pthread_rwlock_unlock(&context->rwlock_layers);
        } else {
            assert(0);
        }
        if (ret != 0 && op.bare_op.fake_req == 0) {
            OpReply *op_reply = malloc(sizeof *op_reply);
            if (op_reply == NULL) {
                return 0;
            }
            ErrorOpReply * const error_op_reply =
                &op_reply->error_op_reply;
            yajl_gen json_gen;                
            
            *error_op_reply = (ErrorOpReply) {
                .type = OP_TYPE_ERROR,
                .req = op.bare_op.req,
                .op_tid = op.bare_op.op_tid,
                .json_gen = NULL                        
            };
            if ((json_gen = new_json_gen(op_reply)) == NULL) {
                free(op_reply);
                return 0;
            }
            error_op_reply->json_gen = json_gen;
            yajl_gen_string(json_gen, (const unsigned char *) "error",
                            (unsigned int) sizeof "error" - (size_t) 1U);
            yajl_gen_string(json_gen, (const unsigned char *) "error",
                            (unsigned int) sizeof "error" - (size_t) 1U);
            send_op_reply(context, op_reply);                
        }
    } else {
        pthread_cond_wait(&context->cond_cqueue, &context->mtx_cqueue);
        goto stab;
    }
    return 0;
}

static void *worker_thread(void *context_)
{
    HttpHandlerContext * const context = context_;
    uint32_t thr_id;
    
    evutil_secure_rng_get_bytes(&thr_id, sizeof thr_id);
    logfile(context, LOG_INFO,
            "Starting worker thread: [%" PRIu32 "]", thr_id);
    
    while (worker_do_work(context) == 0);
    
    logfile(context, LOG_INFO, "Exited worker thread: [%" PRIu32 "]", thr_id);
    
    return NULL;
}

static int process_api_request(HttpHandlerContext *context,
                               const char * const uri,
                               struct evhttp_request * const req,
                               const _Bool fake_req)
{
    char *decoded_uri;
    size_t decoded_uri_len;
    const char *ext = ".json";
    const size_t ext_len = strlen(ext);
    char *domain;
    char *pnt;
    
    decoded_uri = evhttp_decode_uri(uri + context->encoded_api_base_uri_len);
    char * opts;
    if ((opts = strchr(decoded_uri, '?')) != NULL) {
        *opts = 0;
        if (*++opts == 0) {
            opts = NULL;
        }
    }
    decoded_uri_len = strlen(decoded_uri);
    if (decoded_uri_len <= ext_len || decoded_uri_len > MAX_URI_LEN ||
        strcasecmp(decoded_uri + decoded_uri_len - ext_len, ext) != 0) {
        free(decoded_uri);
        if (fake_req == 0) {        
            evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        }
        return -1;
    }
    *(decoded_uri + decoded_uri_len - ext_len) = 0;
    domain = decoded_uri;    
    if ((pnt = strchr(domain, '/')) != NULL) {
        *pnt = 0;
    }
    static Domain domains[] = {
        { .uri_part = "records", .domain_handler = handle_domain_records },
        { .uri_part = "search",  .domain_handler = handle_domain_search },
        { .uri_part = "layers",  .domain_handler = handle_domain_layers },
        { .uri_part = "system",  .domain_handler = handle_domain_system },
        { .uri_part = NULL,      .domain_handler = NULL }
    };
    const Domain *scanned_domain = domains;
    do {
        if (strcasecmp(scanned_domain->uri_part, domain) == 0) {
            break;
        }
        scanned_domain++;
    } while (scanned_domain->uri_part != NULL);
    if (scanned_domain->uri_part == NULL) {
        free(decoded_uri);
        if (fake_req == 0) {        
            evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
        }
        return -1;
    }
    char * const uri_part = domain + strlen(domain) + (size_t) 1U;
    _Bool write_to_log = 0;
    const int ret = scanned_domain->domain_handler(req, context,
                                                   uri_part, opts,
                                                   &write_to_log,
                                                   fake_req);
    free(decoded_uri);
    if (ret != 0) {
        if (fake_req == 0) {        
            evhttp_send_error(req, ret, "Error");
        }
        return- 1;
    }
    if (fake_req == 0 && write_to_log != 0) {
        add_to_db_log(context->now, req->type, uri,
                      evhttp_request_get_input_buffer(req));
    }
    return 0;    
}

static int process_request(HttpHandlerContext *context,
                           struct evhttp_request * const req,
                           const _Bool fake_req)
{
    const char * const uri = evhttp_request_get_uri(req);
    if (strncmp(uri, context->encoded_api_base_uri,
                context->encoded_api_base_uri_len) == 0) {
        return process_api_request(context, uri, req, fake_req);
    }
    if (fake_req == 0) {
        evhttp_send_error(req, HTTP_NOTFOUND, "Not Found");
    }
    return -1;
}

static void http_dispatcher_cb(struct evhttp_request * const req,
                               void * const context_)
{
    HttpHandlerContext *context = context_;
    
    evhttp_connection_set_closecb(evhttp_request_get_connection(req),
                                  http_close_cb, req);
    evhttp_add_header(req->output_headers, "X-Server", SERVER_NAME);
    evhttp_add_header(req->output_headers, "Content-Type",
                      "application/json; charset=UTF-8");
#ifdef NO_KEEPALIVE
    evhttp_add_header(req->output_headers, "Connection", "close");
#endif
    evhttp_add_header(req->output_headers, "Cache-Control",
                      "no-store, private, must-revalidate, proxy-revalidate, "
                      "post-check=0, pre-check=0, max-age=0, s-maxage=0");
    evhttp_add_header(req->output_headers, "Pragma", "no-cache");
    process_request(context, req, 0);
}

int fake_request(HttpHandlerContext * const context,
                 const int verb, char * const uri,
                 const char *body, const size_t body_len)
{
    struct evhttp_request req;
    
    memset(&req, 0, sizeof req);
    req.type = verb;
    req.uri = uri;
    req.input_buffer = evbuffer_new();
    evbuffer_add(req.input_buffer, body, body_len);
    process_request(context, &req, 1);
    evbuffer_free(req.input_buffer);
    worker_do_work(context);
    
    return 0;
}

static void free_layer_slab_entry_cb(void *layer_)
{
    Layer * const layer = layer_;
    free(layer->name);
    layer->name = NULL;
    free_pan_db(&layer->pan_db);
}

static void free_expirables_slab_entry_cb(void *expirable_)
{
    Expirable * const expirable = expirable_;
    *expirable = (Expirable) {
        .ts = (time_t) 0,
        .key_node = NULL
    };
}

static RETSIGTYPE sigterm_cb(const int sig)
{
    (void) sig;
    if (app_context.db_log.journal_rewrite_process != (pid_t) -1) {
        kill(app_context.db_log.journal_rewrite_process, SIGKILL);
        app_context.db_log.journal_rewrite_process = (pid_t) -1;
    }
    if (event_base != NULL) {
        event_base_loopbreak(event_base);
    }
}

static RETSIGTYPE sigchld_cb(const int sig)
{
    (void) sig;
    
    for (;;) {
        int status;
        pid_t pid;

        pid = waitpid((pid_t) -1, &status, WNOHANG);
        if (pid == (pid_t) -1 || pid == (pid_t) 0) {
            break;
        }
        if (WIFEXITED(status) == 0 || WEXITSTATUS(status) != 0) {
            continue;
        }
        assert(app_context.db_log.journal_rewrite_process != (pid_t) -1);
        assert(pid == app_context.db_log.journal_rewrite_process);
        if (pid == app_context.db_log.journal_rewrite_process) {
            system_rewrite_after_fork_cb();       
        }
    }        
}

static void set_signals(void)
{
    sigset_t sigs;
    struct sigaction sa;
    
    sigfillset(&sigs);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGURG, &sa, NULL);
    sa.sa_handler = sigchld_cb;
    sigdelset(&sigs, SIGCHLD);
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_flags = 0;    
    sa.sa_handler = sigterm_cb;
    sigdelset(&sigs, SIGTERM);
    sigaction(SIGTERM, &sa, NULL);
    sigdelset(&sigs, SIGHUP);
    sigaction(SIGHUP, &sa, NULL);
    sigdelset(&sigs, SIGQUIT);
    sigaction(SIGQUIT, &sa, NULL);
    sigdelset(&sigs, SIGINT);
    sigaction(SIGINT, &sa, NULL);
#ifdef SIGXCPU
    sigdelset(&sigs, SIGXCPU);
    sigaction(SIGXCPU, &sa, NULL);
#endif
    sigprocmask(SIG_SETMASK, &sigs, NULL);    
}

int start_workers(HttpHandlerContext * const context)
{
    assert(context->thr_workers == NULL);
    context->should_exit = 0;
    unsigned int nb_workers = app_context.nb_workers;
    assert(nb_workers > 0U);
    pthread_t *thr_workers = malloc(nb_workers * sizeof *thr_workers);
    if (thr_workers == NULL) {
        return -1;
    }
    context->thr_workers = thr_workers;    
    unsigned int t = nb_workers;
    while (t-- > 0U) {
        pthread_create(&thr_workers[t], NULL, worker_thread, context);
    }
    return 0;
}

int stop_workers(HttpHandlerContext * const context)
{
    pthread_t * const thr_workers = context->thr_workers;
    context->should_exit = 1;
    context->thr_workers = thr_workers;    
    pthread_cond_broadcast(&context->cond_cqueue);
    unsigned int t = app_context.nb_workers;
    while (t-- > 0U) {
        pthread_join(thr_workers[t], NULL);
    }
    free(thr_workers);
    context->thr_workers = NULL;
    
    return 0;
}

static void flush_log_db(evutil_socket_t fd, short event,
                         void * const context_)
{
    HttpHandlerContext * const context = context_;
    
    (void) event;
    (void) fd;
    flush_db_log(1);
    struct timeval tv = {
        .tv_sec = app_context.db_log.fsync_period,
        .tv_usec = 0L
    };
    evtimer_add(&context->ev_flush_log_db, &tv);
}

static void expiration_cron(evutil_socket_t fd, short event,
                            void * const context_)
{
    HttpHandlerContext * const context = context_;
    
    (void) event;
    (void) fd;
    if (time(&context->now) == (time_t) -1) {
        return;
    }
    struct timeval tv = {
        .tv_sec = 1L,
        .tv_usec = 0L
    };
#if SPREAD_EXPIRATION
    const int ret = purge_expired_keys(context);
    if (ret != 0 && context->nb_layers > (size_t) 1U) {
        const long required_usec = 1000000L / (long) context->nb_layers;
        if (required_usec < 10000L) {
            while (purge_expired_keys(context) > 0);
        } else {
            tv = (struct timeval) {
                .tv_sec = 0L,
                .tv_usec = required_usec
            };
        }
    }
#else
    purge_expired_keys(context);
#endif
    evtimer_add(&context->ev_expiration_cron, &tv);
}

static int open_log_file(HttpHandlerContext * const context)
{
    int flags = O_RDWR | O_CREAT | O_APPEND;
#ifdef O_EXLOCK
    flags |= O_EXLOCK;
#endif
#ifdef O_NOATIME
    flags |= O_NOATIME;
#endif
#ifdef O_LARGEFILE
    flags |= O_LARGEFILE;
#endif
    assert(context->log_fd == -1);
    if (app_context.log_file_name == NULL) {
        return 0;
    }
    context->log_fd = open(context->log_file_name, flags, (mode_t) 0600);
    if (context->log_fd == -1) {
        logfile(NULL, LOG_ERR, "Can't open [%s]: [%s]", context->log_file_name,
                strerror(errno));
        return -1;
    }
    return 0;
}

static int close_log_file(HttpHandlerContext * const context)
{
    if (context->log_fd != -1) {
        fsync(context->log_fd);
        close(context->log_fd);
        context->log_fd = -1;
    }
    return 0;
}

int http_server(void)
{
    HttpHandlerContext http_handler_context = {
        .should_exit = 0,
        .thr_workers = NULL,
        .event_base = NULL,
        .op_tid = (OpTID) 0U,
        .encoded_api_base_uri = NULL,
        .cqueue = NULL,
        .publisher_bev = NULL,
        .consumer_bev = NULL,
        .nb_layers = (size_t) 0U,
        .log_file_name = NULL,
        .log_fd = -1
    };
    if (time(&http_handler_context.now) == (time_t) -1) {
        return -1;
    }
    http_handler_context.log_file_name = app_context.log_file_name;    
    if (open_log_file(&http_handler_context) != 0) {
        return -1;
    }
    logfile_noformat(&http_handler_context, LOG_INFO,
                     PACKAGE_STRING " started.");
    struct evhttp *event_http;
    struct bufferevent *bev_pair[2];
    set_signals();
    if (init_slab(&http_handler_context.layers_slab,
                  sizeof(Layer), "layers") != 0) {
        return -1;
    }
    if (init_slab(&http_handler_context.expirables_slab,
                  sizeof(Expirable), "expirables") != 0) {
        return -1;
    }
    http_handler_context.cqueue = malloc(sizeof *http_handler_context.cqueue);
    if (http_handler_context.cqueue == NULL ||
        init_cqueue(http_handler_context.cqueue,
                    app_context.max_queued_replies, sizeof(Op)) != 0) {
        return -1;
    }
    pthread_mutex_init(&http_handler_context.mtx_cqueue, NULL);
    pthread_cond_init(&http_handler_context.cond_cqueue, NULL);
    pthread_rwlock_init(&http_handler_context.rwlock_layers, NULL);
    http_handler_context.encoded_api_base_uri = ENCODED_API_BASE_URI;
    http_handler_context.encoded_api_base_uri_len =
        strlen(http_handler_context.encoded_api_base_uri);
    evthread_use_pthreads();
    event_base = event_base_new();
    http_handler_context.event_base = event_base;
    if (bufferevent_pair_new(event_base,
                             BEV_OPT_CLOSE_ON_FREE |
                             BEV_OPT_THREADSAFE |
                             BEV_OPT_DEFER_CALLBACKS, bev_pair) != 0) {
        return -1;
    }
    bufferevent_enable(bev_pair[0], EV_READ);
    bufferevent_disable(bev_pair[1], EV_READ);
    bufferevent_setwatermark(bev_pair[0], EV_READ, sizeof(struct OpReply *),
                             sizeof(struct OpReply *) * (size_t) 100U);
    http_handler_context.consumer_bev = bev_pair[0];
    http_handler_context.publisher_bev = bev_pair[1];    
    event_http = evhttp_new(event_base);
    evhttp_bind_socket(event_http,
                       app_context.server_ip,
                       (unsigned short) strtoul(app_context.server_port,
                                                NULL, 10));
    evhttp_set_timeout(event_http, app_context.timeout);
    evhttp_set_gencb(event_http, http_dispatcher_cb, &http_handler_context);
    bufferevent_setcb(http_handler_context.consumer_bev,
                      consumer_cb, NULL, NULL, &http_handler_context);
    if (app_context.db_log.fsync_period > 0) {
        evtimer_assign(&http_handler_context.ev_flush_log_db, event_base,
                       flush_log_db, &http_handler_context);
        struct timeval tv = {
            .tv_sec = app_context.db_log.fsync_period,
            .tv_usec = 0L
        };
        evtimer_add(&http_handler_context.ev_flush_log_db, &tv);
    }
    replay_log(&http_handler_context);
    if (start_workers(&http_handler_context) != 0) {
        goto bye;
    }
    evtimer_assign(&http_handler_context.ev_expiration_cron, event_base,
                   expiration_cron, &http_handler_context);
    struct timeval tv = {
        .tv_sec = 0L,
        .tv_usec = 0L
    };
    evtimer_add(&http_handler_context.ev_expiration_cron, &tv);
    
    event_base_dispatch(event_base);
    
    stop_workers(&http_handler_context);
    if (app_context.db_log.fsync_period > 0) {
        evtimer_del(&http_handler_context.ev_flush_log_db);
    }
    evtimer_del(&http_handler_context.ev_expiration_cron);
bye:
    bufferevent_free(http_handler_context.publisher_bev);
    bufferevent_free(http_handler_context.consumer_bev);
    evhttp_free(event_http);
    event_base_free(event_base);
    free_cqueue(http_handler_context.cqueue);
    pthread_cond_destroy(&http_handler_context.cond_cqueue);
    pthread_mutex_destroy(&http_handler_context.mtx_cqueue);
    pthread_rwlock_destroy(&http_handler_context.rwlock_layers);
    free_slab(&http_handler_context.layers_slab, free_layer_slab_entry_cb);
    free_slab(&http_handler_context.expirables_slab,
              free_expirables_slab_entry_cb);
    close_log_file(&http_handler_context);
    
    return 0;
}
