
#ifndef __SLIPMAP_H__
#define __SLIPMAP_H__ 1

typedef struct SlipMap_ {
    size_t sizeof_map;    
    unsigned char map[];
} SlipMap;

typedef struct SlipMap * SlipMapPnt;

SlipMap *new_slip_map(size_t buffer_size);
void free_slip_map(SlipMap * * const slip_map_pnt);

int add_entry_to_slip_map(SlipMap * * const slip_map_pnt,
                          void * const key, const size_t key_len,
                          void * const value, const size_t value_len);

int replace_entry_in_slip_map(SlipMap * * const slip_map_pnt,
                              void * const key, const size_t key_len,
                              void * const value, const size_t value_len);

int remove_entry_from_slip_map(SlipMap * * const slip_map_pnt,
                               void * const key, const size_t key_len);

typedef int (*SlipMapForeachCB)(void *context,
                                const void * const key,
                                const size_t key_len,
                                const void * const value,
                                const size_t value_len);

int slip_map_foreach(SlipMap * * const slip_map_pnt, SlipMapForeachCB cb,
                     void * const context);

int find_in_slip_map(SlipMap * * const slip_map_pnt,
                     const void * const key, const size_t key_len,
                     const void * * const value, size_t * const value_len);

int dump_slip_map(SlipMap * const * const slip_map_pnt);

#endif
