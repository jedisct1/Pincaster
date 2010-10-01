
#ifndef __KEYS_H__
#define __KEYS_H__ 1

typedef struct Key_ {
    size_t len;
    unsigned int ref_count;
    char val[];
} Key;

int init_key(Key * const key);
void free_key(Key * const key);
int retain_key(Key * const key);
void release_key(Key * const key);
Key *new_key(const void * const val, const size_t len);
Key *new_key_from_c_string(const char *ckey);
Key *new_key_from_uri_encoded_c_string(const char * const uckey);
Key *new_key_with_leading_zero(const void * const val, const size_t len);

#endif
