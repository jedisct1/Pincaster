
#include "common.h"
#include "slipmap.h"

static int init_slip_map(SlipMap * const slip_map)
{
    unsigned char *map = slip_map->map;
    size_t slot_size;

    assert(slip_map->sizeof_map >= (size_t) 6U);
    slot_size = slip_map->sizeof_map - (size_t) 6U;
    map[0] = (unsigned char) (slot_size >> 8);
    map[1] = (unsigned char) (slot_size & 0xff);
    map[2] = map[3] = map[4] = map[5] = 0U;
    
    return 0;
}

SlipMap *new_slip_map(size_t buffer_size)
{
    SlipMap *slip_map;
    size_t sizeof_map;
    size_t sizeof_slip_map;
    
    if (buffer_size < sizeof *slip_map + (size_t) 6U) {
        buffer_size = sizeof *slip_map + (size_t) 6U;
    }
    sizeof_map = buffer_size - sizeof *slip_map;
    if (sizeof_map > (size_t) 0xffff + (size_t) 6U) {
        return NULL;
    }
    sizeof_slip_map = sizeof *slip_map + sizeof_map;
    if ((slip_map = malloc(sizeof_slip_map)) == NULL) {
        return NULL;
    }
    slip_map->sizeof_map = sizeof_map;
    init_slip_map(slip_map);
    
    return slip_map;
}

void free_slip_map(SlipMap * * const slip_map_pnt)
{
    unsigned char *map;
    SlipMap *slip_map;
    
    if (slip_map_pnt == NULL) {
        return;
    }
    slip_map = *slip_map_pnt;
    if (slip_map == NULL) {
        return;
    }    
    map = slip_map->map;
    map[0] = map[1] = map[2] = map[3] = map[4] = map[5] = 0U;
    slip_map->sizeof_map = (size_t) 0U;    
    free(slip_map);
    *slip_map_pnt = NULL;
}

static int update_map_slot(unsigned char * pnt,
                           const size_t new_slot_free,
                           const unsigned char * const key,
                           const size_t key_len,
                           const unsigned char * const value,
                           const size_t value_len)
{
    *pnt++ = (unsigned char) (new_slot_free >> 8);
    *pnt++ = (unsigned char) (new_slot_free & 0xff);            
    *pnt++ = (unsigned char) (key_len >> 8);
    *pnt++ = (unsigned char) (key_len & 0xff);
    *pnt++ = (unsigned char) (value_len >> 8);
    *pnt++ = (unsigned char) (value_len & 0xff);
    memcpy(pnt, key, key_len);
    pnt += key_len;
    memcpy(pnt, value, value_len);
    
    return 0;
}

int add_entry_to_slip_map(SlipMap * * const slip_map_pnt,
                          void * const key, const size_t key_len,
                          void * const value, const size_t value_len)
{
    SlipMap *slip_map;
    size_t slot_free = (size_t) 0U;
    size_t new_slot_free;
    size_t required_space;
    size_t scanned_key_len = (size_t) 0U;
    size_t scanned_value_len = (size_t) 0U;
    unsigned char *pnt;
    unsigned char *slot_free_pnt;
    unsigned char *new_map;
    size_t sizeof_new_map;
    size_t last_slot_wanted_free;

    assert(key_len > (size_t) 0U);
    assert(key_len <= 0xffff);    
    assert(value_len <= 0xffff);
    if (key_len > (size_t) 0xffff || key_len <= (size_t) 0U ||
        value_len > 0xffff) {
        return -1;
    }    
    assert(slip_map_pnt != NULL);
    slip_map = *slip_map_pnt;    
    assert(slip_map != NULL);
    assert(slip_map->map != NULL);
    pnt = slip_map->map;
    required_space = (size_t) 6U + key_len + value_len;
    retry:
    slot_free_pnt = NULL;
    while (pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map) {
        slot_free_pnt = pnt;        
        slot_free  = ((size_t) *pnt++) << 8;
        slot_free |=  (size_t) *pnt++;
        scanned_key_len  = ((size_t) *pnt++) << 8;
        scanned_key_len |=  (size_t) *pnt++;
        scanned_value_len  = ((size_t) *pnt++) << 8;
        scanned_value_len |= (size_t) *pnt++;
        if (scanned_key_len <= (size_t) 0U) {
            assert(required_space >= (size_t) 6U);
            required_space -= (size_t) 6U;
            assert(scanned_key_len == (size_t) 0U);
            assert(scanned_value_len == (size_t) 0U);
            if (slot_free >= required_space) {
                pnt = slot_free_pnt;
                assert(slot_free >= key_len + value_len);
                new_slot_free = slot_free - key_len - value_len;
                int ret = update_map_slot(pnt, new_slot_free,
                                          key, key_len, value, value_len);
                return ret;
            }
        }        
        pnt += scanned_key_len;
        pnt += scanned_value_len;
        if (slot_free >= required_space) {
            slot_free_pnt[0] = slot_free_pnt[1] = 0U;
            new_slot_free = slot_free - required_space;            
            
            int ret = update_map_slot(pnt, new_slot_free,
                                      key, key_len, value, value_len);
            return ret;
        }
        pnt += slot_free;
    }
    if (scanned_key_len <= (size_t) 0U) {
        required_space += (size_t) 6U;
    }
    assert(slot_free_pnt != NULL);
    last_slot_wanted_free = required_space;
    assert(slot_free < last_slot_wanted_free);
    sizeof_new_map = slip_map->sizeof_map +
        last_slot_wanted_free - slot_free;
    if (sizeof_new_map > (size_t) 0xffff) {
        return -1;
    }
    SlipMap *new_slip_map;
    const size_t sizeof_new_slip_map = sizeof_new_map + sizeof *new_slip_map;
    new_slip_map = realloc(slip_map, sizeof_new_slip_map);
    if (new_slip_map == NULL) {
        return -1;
    }
    new_map = new_slip_map->map;
    if (scanned_key_len <= (size_t) 0U) {
        slot_free_pnt = new_map;
    } else {
        slot_free_pnt = new_map + (slot_free_pnt - slip_map->map);
    }
    slip_map = new_slip_map;
    *slip_map_pnt = slip_map;
    slip_map->sizeof_map = sizeof_new_map;
    slot_free_pnt[0] = (unsigned char) (last_slot_wanted_free >> 8);
    slot_free_pnt[1] = (unsigned char) (last_slot_wanted_free & 0xff);
    pnt = slot_free_pnt;    
    assert(pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map);
    goto retry;
    
    /* NOTREACHED */
}

int replace_entry_in_slip_map(SlipMap * * const slip_map_pnt,
                              void * const key, const size_t key_len,
                              void * const value, const size_t value_len)
{
    SlipMap *slip_map;
    size_t slot_free;
    size_t new_slot_free;
    size_t required_space;
    size_t scanned_key_len;
    size_t scanned_value_len;
    unsigned char *pnt;
    unsigned char *slot_free_pnt;

    assert(key_len > (size_t) 0U);
    assert(key_len <= 0xffff);    
    assert(value_len <= 0xffff);
    if (key_len > (size_t) 0xffff || key_len <= (size_t) 0U ||
        value_len > (size_t) 0xffff) {
        return -1;
    }
    assert(slip_map_pnt != NULL);
    slip_map = *slip_map_pnt;    
    assert(slip_map != NULL);
    assert(slip_map->map != NULL);
    pnt = slip_map->map;
    required_space = (size_t) 6U + key_len + value_len;
    
    while (pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map) {
        slot_free_pnt = pnt;
        slot_free  = ((size_t) *pnt++) << 8;
        slot_free |=  (size_t) *pnt++;
        scanned_key_len  = ((size_t) *pnt++) << 8;
        scanned_key_len |=  (size_t) *pnt++;
        scanned_value_len  = ((size_t) *pnt++) << 8;
        scanned_value_len |= (size_t) *pnt++;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_key_len == (size_t) 0U);
            assert(scanned_value_len == (size_t) 0U);
        }
        if (scanned_key_len == key_len && memcmp(pnt, key, key_len) == 0) {
            if (required_space > (size_t) 6U + scanned_key_len +
                scanned_value_len + slot_free) {
                if (remove_entry_from_slip_map(slip_map_pnt,
                                               key, key_len) < 0) {
                    return -1;
                }
                return add_entry_to_slip_map(slip_map_pnt, key, key_len,
                                             value, value_len);
            }
            pnt += scanned_key_len;
            new_slot_free = (size_t) 6U + scanned_key_len +
                scanned_value_len + slot_free - required_space;
            memmove(pnt, value, value_len);
            slot_free_pnt[0] = (unsigned char) (new_slot_free >> 8);
            slot_free_pnt[1] = (unsigned char) (new_slot_free & 0xff);
            slot_free_pnt[4] = (unsigned char) (value_len >> 8);
            slot_free_pnt[5] = (unsigned char) (value_len & 0xff);
            
            return 0;
        }
        pnt += scanned_key_len;
        pnt += scanned_value_len;
        pnt += slot_free;
    }    
    return add_entry_to_slip_map(slip_map_pnt, key, key_len, value, value_len);
}

int remove_entry_from_slip_map(SlipMap * * const slip_map_pnt,
                               void * const key, const size_t key_len)
{
    SlipMap *slip_map;
    size_t slot_free;
    size_t previous_slot_free;
    size_t new_slot_free;
    size_t scanned_key_len;
    size_t scanned_value_len;
    unsigned char *pnt;
    unsigned char *slot_free_pnt;
    unsigned char *previous_slot;
    
    assert(key_len > (size_t) 0U);
    assert(key_len <= 0xffff);    
    if (key_len > (size_t) 0xffff || key_len <= (size_t) 0U) {
        return -1;
    }
    assert(slip_map_pnt != NULL);    
    slip_map = *slip_map_pnt;
    assert(slip_map != NULL);
    assert(slip_map->map != NULL);
    pnt = slip_map->map;
    previous_slot = NULL;
    while (pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map) {
        slot_free_pnt = pnt;
        slot_free  = ((size_t) *pnt++) << 8;
        slot_free |=  (size_t) *pnt++;
        scanned_key_len  = ((size_t) *pnt++) << 8;
        scanned_key_len |=  (size_t) *pnt++;
        scanned_value_len  = ((size_t) *pnt++) << 8;
        scanned_value_len |= (size_t) *pnt++;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_key_len == (size_t) 0U);
            assert(scanned_value_len == (size_t) 0U);
        }
        if (scanned_key_len == key_len && memcmp(pnt, key, key_len) == 0) {
            if (previous_slot == NULL) {
                size_t shift;

                pnt += scanned_key_len;
                pnt += scanned_value_len;
                pnt += slot_free;                
                shift = (size_t) (pnt - slip_map->map);
                if (shift == slip_map->sizeof_map) {
                    init_slip_map(slip_map);
                    return 1;
                }
                assert(shift <= slip_map->sizeof_map);
                slip_map->sizeof_map -= shift;
                memmove(slip_map->map, pnt, slip_map->sizeof_map);
                
                SlipMap *new_slip_map;                
                const size_t sizeof_new_slip_map = slip_map->sizeof_map +
                    sizeof *new_slip_map;
                new_slip_map = realloc(slip_map, sizeof_new_slip_map);
                if (new_slip_map == NULL) {
                    return 1;
                }
                *slip_map_pnt = new_slip_map;
                
                return 1;
            }
            previous_slot_free  = ((size_t) previous_slot[0]) << 8;
            previous_slot_free |=  (size_t) previous_slot[1];
            new_slot_free = previous_slot_free + (size_t) 6U +
                scanned_key_len + scanned_value_len + slot_free;
            previous_slot[0] = (unsigned char) (new_slot_free >> 8);
            previous_slot[1] = (unsigned char) (new_slot_free & 0xff);
            slot_free_pnt[0] = slot_free_pnt[1] = slot_free_pnt[2] =
            slot_free_pnt[3] = slot_free_pnt[4] = slot_free_pnt[5] = 0U;
            
            return 1;
        }
        previous_slot = slot_free_pnt;
        pnt += scanned_key_len;
        pnt += scanned_value_len;
        pnt += slot_free;
    }
    return 0;
}

int slip_map_foreach(SlipMap * * const slip_map_pnt, SlipMapForeachCB cb,
                     void * const context)
{
    const SlipMap *slip_map;
    size_t slot_free;
    size_t scanned_key_len;
    size_t scanned_value_len;
    const unsigned char *pnt;
    const char *scanned_key;
    const char *scanned_value;

    if (slip_map_pnt == NULL) {
        return 0;
    }
    slip_map = *slip_map_pnt;
    if (slip_map == NULL) {
        return 0;
    }
    pnt = slip_map->map;    
    while (pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map) {
        slot_free  = ((size_t) *pnt++) << 8;
        slot_free |=  (size_t) *pnt++;
        scanned_key_len  = ((size_t) *pnt++) << 8;
        scanned_key_len |=  (size_t) *pnt++;
        scanned_value_len  = ((size_t) *pnt++) << 8;
        scanned_value_len |= (size_t) *pnt++;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_key_len == (size_t) 0U);
            assert(scanned_value_len == (size_t) 0U);
        }
        scanned_key = (const char *) pnt;
        pnt += scanned_key_len;
        scanned_value = (const char *) pnt;
        pnt += scanned_value_len;
        pnt += slot_free;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_value_len == (size_t) 0U);
            continue;
        } else {
            if (cb(context, scanned_key, scanned_key_len,
                   scanned_value, scanned_value_len) != 0) {
                break;
            }
        }
    }    
    return 0;
}

typedef struct FindInSlipMapCBContext_ {
    const void *key;
    size_t key_len;
    const void *value;
    size_t value_len;
} FindInSlipMapCBContext;

static int find_in_slip_map_cb(void * const context_,
                               const void * const key,
                               const size_t key_len,
                               const void * const value,
                               const size_t value_len)
{
    FindInSlipMapCBContext * const context = context_;
    
    if (key_len == context->key_len &&
        memcmp(key, context->key, key_len) == 0) {
        context->value = value;
        context->value_len = value_len;
        return 1;
    }
    return 0;
}

int find_in_slip_map(SlipMap * * const slip_map_pnt,
                     const void * const key, const size_t key_len,
                     const void * * const value, size_t * const value_len)
{
    FindInSlipMapCBContext context = {
        .key = key,    .key_len = key_len,
        .value = NULL, .value_len = (size_t) 0U
    };
    slip_map_foreach(slip_map_pnt, find_in_slip_map_cb, &context);
    *value = context.value;
    *value_len = context.value_len;    
    if (*value == NULL) {
        return 0;
    }
    return 1;
}

#ifdef DEBUG
int dump_slip_map(SlipMap * const * const slip_map_pnt)
{
    const SlipMap *slip_map;
    size_t slot_free;
    size_t scanned_key_len;
    size_t scanned_value_len;
    const unsigned char *pnt;
    const char *scanned_key;
    const char *scanned_value;

    if (slip_map_pnt == NULL || *slip_map_pnt == NULL) {
        return 0;
    }
    slip_map = *slip_map_pnt;
    puts("\n---------\n");
    printf("Map size: %zu\n", (*slip_map_pnt)->sizeof_map);    
    assert(slip_map != NULL);
    assert(slip_map->map != NULL);
    pnt = slip_map->map;
    while (pnt - slip_map->map < (ptrdiff_t) slip_map->sizeof_map) {
        slot_free  = ((size_t) *pnt++) << 8;
        slot_free |=  (size_t) *pnt++;
        scanned_key_len  = ((size_t) *pnt++) << 8;
        scanned_key_len |=  (size_t) *pnt++;
        scanned_value_len  = ((size_t) *pnt++) << 8;
        scanned_value_len |= (size_t) *pnt++;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_key_len == (size_t) 0U);
            assert(scanned_value_len == (size_t) 0U);
        }
        scanned_key = (const char *) pnt;
        pnt += scanned_key_len;
        scanned_value = (const char *) pnt;
        pnt += scanned_value_len;
        pnt += slot_free;
        if (scanned_key_len <= (size_t) 0U) {
            assert(scanned_value_len == (size_t) 0U);
            printf("(empty, free: [%zu])\n", slot_free);
            fflush(stdout);
        } else {
            if (write(STDOUT_FILENO, "[", sizeof "[" - (size_t) 1U) <= 0 ||
                write(STDOUT_FILENO, scanned_key, scanned_key_len) <= 0 ||
                write(STDOUT_FILENO, "] => [", sizeof "] => [" - (size_t) 1U) <= 0 ||
                write(STDOUT_FILENO, scanned_value, scanned_value_len) <= 0 ||
                write(STDOUT_FILENO, "] ", sizeof "] " - (size_t) 1U) <= 0) {
                return 0;
            }
            printf("(free: [%zu])\n", slot_free);
            fflush(stdout);
        }
    }
    return 0;
}
#endif
