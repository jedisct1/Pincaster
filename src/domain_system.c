
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

int rewrite_child(HttpHandlerContext * const context)
{
    (void) context;
    puts("Child");
    return 0;
}

static int handle_special_op_system_rewrite(HttpHandlerContext * const context,
                                            SystemRewriteOp * const rewrite_op)
{
    if (rewrite_op->fake_req != 0) {
        return 0;
    }
    stop_workers(context);
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

