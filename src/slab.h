
#ifndef __SLAB_H__
#define __SLAB_H__ 1

typedef struct Slab_ {
    size_t nb_slots_per_chunk;
    size_t sizeof_entry;
    size_t alignment_gap;
    size_t alignment_gap_before_data;
    size_t sizeof_slab_slot;
    struct SlabChunk_ *first_partial_chunk;
    struct SlabChunk_ *first_full_chunk;
    const void *name;
} Slab;

typedef int (*SlabForeachCB)(void *context, void *entry,
                             const size_t sizeof_entry);

typedef void (*FreeSlabEntryCB)(void *entry);

int init_slab(Slab * const slab, const size_t sizeof_entry,
              const void * const name);

void free_slab(Slab * const slab, FreeSlabEntryCB free_cb);
int slab_foreach(Slab * const slab, SlabForeachCB cb, void * const context);

void *add_entry_to_slab(Slab * const slab, const void * const entry);
int remove_entry_from_slab(Slab * const slab, void * const entry);

#endif

