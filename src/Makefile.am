
pincaster_LDADD = \
        levent2/.libs/libevent_core.a \
        levent2/.libs/libevent_extra.a \
        levent2/.libs/libevent_pthreads.a \
        @YAJL_LDADD@

AM_CFLAGS = \
        @YAJL_INCLUDE@

bin_PROGRAMS = \
        pincaster
        
pincaster_SOURCES = \
        common.h \
        ext/queue.h \
        ext/tree.h \
        app.c \
        parser.c \
        parser_p.h \
        parser.h \
        app_config.c \
        app_config.h \
        utils.c \
        utils.h \
        log.c \
        log.h \
        cqueue.c \
        cqueue.h \
        keys.c \
        keys.h \
        pandb.c \
        pandb.h \
        slab.c \
        slab.h \
        slab_p.h \
        slipmap.c \
        slipmap.h \
        stack.c \
        stack.h \
        db_log.c \
        db_log.h \
        query_parser.c \
        query_parser.h \
        http_server.c \
        http_server.h \
        key_nodes.c \
        key_nodes.h \
        expirables.c \
        expirables.h \
        domain_system.c \
        domain_system.h \
        domain_layers.c \
        domain_layers.h \
        domain_records.c \
        domain_records.h \
        domain_search.c \
        domain_search.h \
        handle_consumer_ops.c \
        handle_consumer_ops.h \
        public.c \
        public.h \
        replication_master.c \
        replication_master.h

#pincaster_DEPENDS = \
#        yajl/libyajl.a
        
SUBDIRS = \
        ext levent2 yajl
