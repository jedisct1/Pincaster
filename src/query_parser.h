
#ifndef __QUERY_PARSER_H__
#define __QUERY_PARSER_H__ 1

#ifndef MAX_URI_COMPONENTS
# define MAX_URI_COMPONENTS 8U
#endif
#ifndef MAX_QUERY_STRING_VARS
# define MAX_QUERY_STRING_VARS 8U
#endif

typedef int (*QueryParseCB)(void *context,
                            const BinVal *key, const BinVal *value);

typedef struct ParsedURI_ {
    SBinVal uri_components[MAX_URI_COMPONENTS];
    SBinVal query_string_vars[MAX_QUERY_STRING_VARS];
    size_t nb_uri_components;
    size_t nb_query_string_vars;
} ParsedURI;

int query_parse(const char * const query, QueryParseCB cb,
                void * const context);

int uri_encode_binval(BinVal * const evalue, const BinVal * const value);

#endif
