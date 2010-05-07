
#ifndef __QUERY_PARSER_H__
#define __QUERY_PARSER_H__ 1

typedef int (*QueryParseCB)(void *context,
                            const BinVal *key, const BinVal *value);

int query_parse(const char * const query, QueryParseCB cb,
                void * const context);

#endif
