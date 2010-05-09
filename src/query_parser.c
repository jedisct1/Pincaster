
#include "common.h"
#include "query_parser.h"

static size_t query_decode(const char *encoded, size_t encoded_size,
                           char * const decoded, const size_t max_decoded_size)
{
    char c;
    char *pnt_decoded = decoded;
    char hexbuf[3] = { [2] = 0 };
    
    assert(max_decoded_size >= encoded_size);

    while (encoded_size-- > (size_t) 0U) {
        c = *encoded++;
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            if (encoded_size < (size_t) 2U) {
                break;
            }
            hexbuf[0] = *encoded++;
            hexbuf[1] = *encoded++;
            c = (char) strtol(hexbuf, NULL, 16);
            encoded_size -= (size_t) 2U;
        }
        *pnt_decoded++ = c;        
    }
    return (size_t) (pnt_decoded - decoded);
}

static int query_parse_part(const char * const part,
                            BinVal * const key, BinVal * const value,
                            const char * * const next)
{
    const char *pnt;
    const char *ekey;
    const char *evalue;
    size_t ekey_size;
    size_t evalue_size;
    void *tmp_buf;

    *next = NULL;
    ekey = pnt = part;
    for (;;) {
        if (*pnt == 0) {
            return -1;
        }
        if (*pnt == '=') {
            break;
        }
        pnt++;
    }
    ekey_size = (size_t) (pnt - ekey);
    if (key->max_size <= ekey_size) {
        if ((tmp_buf = realloc(key->val, ekey_size + (size_t) 1U)) == NULL) {
            return -1;
        }
        key->val = tmp_buf;
        key->max_size = ekey_size + (size_t) 1U;
    }
    if (ekey_size <= (size_t) 0U || *ekey == '&') {        
        return -1;
    }
    key->size = query_decode(ekey, ekey_size, key->val, key->max_size);
    *(key->val + key->size) = 0;
    
    evalue = ++pnt;
    while (*pnt != 0 && *pnt != '&') {
        pnt++;
    }
    evalue_size = (size_t) (pnt - evalue);
    if (value->max_size <= evalue_size) {
        if ((tmp_buf = realloc(value->val,
                               evalue_size + (size_t) 1U)) == NULL) {
            return -1;
        }
        value->val = tmp_buf;
        value->max_size = evalue_size + (size_t) 1U;
    }
    value->size = query_decode(evalue, evalue_size,
                               value->val, value->max_size);
    *(value->val + value->size) = 0;    
    if (*pnt != 0) {
        *next = ++pnt;        
    }
    return 0;
}

int query_parse(const char * const query, QueryParseCB cb,
                void * const context)
{    
    const char *part;
    const char *next;
    BinVal key = {
        .val = NULL,
        .size = (size_t) 0U,
        .max_size = (size_t) 0U
    };
    BinVal value = key;
    
    assert(query != NULL);
    if (*query == 0) {
        return 0;
    }
    part = query;
    int ret = 0;
    do {
        if (query_parse_part(part, &key, &value, &next) != 0) {
            ret = -1;
            break;
        }
        if (cb != NULL && cb(context, &key, &value) != 0) {
            ret = 1;
            break;
        }
    } while ((part = next) != NULL);
    free(key.val);
    key.val = NULL;
    free(value.val);
    value.val = NULL;
     
    return ret;
}

static const _Bool uri_chars[256] = { // from libevent
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 0, 0, 1, 0, 0, 1,   1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 1, 0, 0,
	/* 64 */
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,
	0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};

int uri_encode_binval(BinVal * const evalue, const BinVal * const value)
{
    size_t size = value->size;
    size_t wanted_max_size;
    unsigned char c;
    char *tmp_buf;
    char *pnt_decoded = value->val;
    char hexbuf[4] = { [3] = 0 };

    evalue->size = (size_t) 0;    
    if (value->size > evalue->max_size || evalue->val == NULL) {
        wanted_max_size = value->size * (size_t) 3 / (size_t) 2;
        if (wanted_max_size <= value->size) {
            exit(1);
        }
        assert(wanted_max_size > (size_t) 0U);
        if ((tmp_buf = realloc(evalue->val,
                               wanted_max_size + (size_t) 1U)) == NULL) {
            return -1;
        }
        evalue->val = tmp_buf;
        evalue->max_size = wanted_max_size;
    }
    while (size-- > (size_t) 0U) {
        c = (unsigned char) *pnt_decoded++;
        if (uri_chars[c] != 0) {
            append_to_binval(evalue, (char *) &c, (size_t) 1U);
        } else {
            snprintf(hexbuf, sizeof hexbuf, "%%%02x", c);
            append_to_binval(evalue, hexbuf, sizeof hexbuf - (size_t) 1U);
        }
    }
    return 0;
}
