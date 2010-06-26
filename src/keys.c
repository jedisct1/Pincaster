
#include "common.h"
#include "keys.h"

int init_key(Key * const key)
{
    key->len = (size_t) 0U;
    key->ref_count = 1U;
    
    return 0;
}

void free_key(Key * const key)
{
    if (key == NULL) {
        return;
    }
    assert(key->ref_count == 0U);
    key->val[0] = 0;
    key->len = (size_t) 0U;
    key->ref_count = 0U;
    free(key);
}

int retain_key(Key * const key)
{
    assert(key->ref_count > 0U);
    if (key->ref_count >= UINT_MAX) {
        assert(key->ref_count < UINT_MAX);
        return -1;
    }
    key->ref_count++;
    
    return 0;
}

void release_key(Key * const key)
{
    if (key == NULL) {
        return;
    }
    assert(key->ref_count > 0U);
    if (key->ref_count <= 0U) {
        return;
    }
    key->ref_count--;
    if (key->ref_count <= 0U) {
        free_key(key);
    }
}

Key *new_key(const void * const val, const size_t len)
{
    Key *key;

    if (len < (size_t) 1U) {
        return NULL;
    }
    if (SIZE_MAX - sizeof *key < len) {
        return NULL;
    }    
    if ((key = malloc(sizeof *key + len)) == NULL) {
        return NULL;
    }
    init_key(key);
    key->len = len;
    memcpy(key->val, val, len);
    
    return key;
}

Key *new_key_from_c_string(const char *ckey)
{
    const size_t len = strlen(ckey);
    
    assert(len > (size_t) 0U);        
    return new_key(ckey, len + (size_t) 1U);
}

Key *new_key_with_leading_zero(const void * const val, const size_t len)
{
    if (len >= SIZE_MAX) {
        return NULL;
    }
    Key *key;    
    const size_t len1 = len + (size_t) 1U;
    if (SIZE_MAX - sizeof *key < len1) {
        return NULL;
    }    
    if ((key = malloc(sizeof *key + len1)) == NULL) {
        return NULL;
    }
    init_key(key);
    key->len = len1;
    memcpy(key->val, val, len);
    *((char *) key->val + len) = 0;
    
    return key;    
}
