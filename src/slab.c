
#include "common.h"
#include "slab.h"
#include "slab_p.h"

#ifndef HAVE_FFSL
static int ffsti2(long long lli)
{
    int i, num, t, tmpint, len;
    
    num = sizeof(long long) / sizeof(int);
    if (num == 1) {
        return ffs((int) lli);
    }
    len = sizeof(int) * 8;    
    for (i = 0; i < num; i++) {
        tmpint = (int) (((lli >> len) << len) ^ lli);        
        t = ffs(tmpint);
        if (t != 0) {
            return t + i * len;
        }
        lli = lli >> len;        
    }
    return 0;
}

static int ffsl(const long li)
{
    return ffsti2((long long) li);
}
#endif

int init_slab(Slab * const slab, const size_t sizeof_entry,
              const void * const name)
{
    SlabSlotFodder fodder;
    SlabSlotFodderFirst fodder_first;
        
    slab->name = name;
    slab->nb_slots_per_chunk = sizeof(BitmapType) * CHAR_BIT;
    slab->sizeof_entry = sizeof_entry;
    slab->alignment_gap =
        (size_t) ((unsigned char *) &fodder.fodder2 -
                  (unsigned char *) &fodder) - sizeof(fodder.fodder1);
    slab->alignment_gap_before_data =
        (size_t) ((unsigned char *) &fodder_first.fodder -
                  (unsigned char *) &fodder_first) -
        sizeof(fodder_first.bitmap);
    slab->sizeof_slab_slot = sizeof(SlabSlot) +
        slab->alignment_gap_before_data +
        slab->sizeof_entry + slab->alignment_gap;
    slab->first_partial_chunk = NULL;
    slab->first_full_chunk = NULL;
    
    return 0;
}

SlabChunk *new_slab_chunk(Slab * const slab)
{
    SlabChunk *chunk;
    size_t sizeof_slab_chunk;
    
    sizeof_slab_chunk = sizeof *chunk +
        slab->sizeof_slab_slot * slab->nb_slots_per_chunk;
    if ((chunk = malloc(sizeof_slab_chunk)) == NULL) {
        return NULL;
    }
    chunk->bitmap = ~(BitmapType) 0U;
    chunk->next = NULL;
    chunk->previous = NULL;    
    
    return chunk;
}

void *add_entry_to_slab(Slab * const slab, const void * const entry)
{
    size_t bit;
    SlabSlot *slot;
    unsigned char *slot_data;
    SlabChunk *chunk;
    
    chunk = slab->first_partial_chunk;
    if (chunk == NULL) {
        chunk = new_slab_chunk(slab);
        chunk->next = slab->first_partial_chunk;
        if (slab->first_partial_chunk != NULL) {
            slab->first_partial_chunk->previous = chunk;
        }
        slab->first_partial_chunk = chunk;        
    }
    bit = (size_t) ffsl((long) chunk->bitmap);
    assert(bit > 0U);
    bit--;
    slot = (SlabSlot *) ((unsigned char *) chunk->unaligned_slots +
                         slab->sizeof_slab_slot * bit);
    slot_data = slot->unaligned_data + slab->alignment_gap_before_data;
    memcpy(slot_data, entry, slab->sizeof_entry);
    slot->slab_chunk = chunk;
    chunk->bitmap &= ~((BitmapType) 1 << bit);
    if (chunk->bitmap == (BitmapType) 0) {
        if (chunk->previous != NULL) {
            chunk->previous->next = chunk->next;
        }
        if (chunk->next != NULL) {
            chunk->next->previous = chunk->previous;
        }
        if (slab->first_partial_chunk == chunk) {
            assert(chunk->previous == NULL);
            slab->first_partial_chunk = NULL;
        }
        chunk->next = slab->first_full_chunk;
        chunk->previous = NULL;
        if (slab->first_full_chunk != NULL) {
            slab->first_full_chunk->previous = chunk;
        }
        slab->first_full_chunk = chunk;
    }
    return slot_data;
}

int remove_entry_from_slab(Slab * const slab, void * const entry)
{
    SlabSlot *slot;
    SlabSlot *scanned_slot;
    SlabChunk *chunk;
    size_t bit;
    _Bool was_full;

    slot = (SlabSlot *) ((unsigned char *) entry - sizeof *slot -
                         slab->alignment_gap_before_data);
    chunk = slot->slab_chunk;
    assert(chunk != NULL);
    if (chunk == NULL) {
        return -1; /* should never happen, but better leak than crash */
    }
    scanned_slot = chunk->unaligned_slots;
    bit = (size_t) 0U;
    for (;;) {
        if (scanned_slot == slot) {
            break;
        }
        scanned_slot = (SlabSlot *)
            ((unsigned char *) scanned_slot + slab->sizeof_slab_slot);
        if (++bit >= slab->nb_slots_per_chunk) {
            return -1;
        }
    }
    scanned_slot->slab_chunk = NULL;
    if (chunk->bitmap == (BitmapType) 0U) {
        was_full = 1;
    } else {
        was_full = 0;
    }
    chunk->bitmap |= ((BitmapType) 1U << bit);
    if (was_full != 0) {
        if (chunk->previous != NULL) {
            chunk->previous->next = chunk->next;
        }
        if (chunk->next != NULL) {
            chunk->next->previous = chunk->previous;
        }
        if (slab->first_full_chunk == chunk) {
            assert(chunk->previous == NULL);
            slab->first_full_chunk = chunk->next;
        }        
        chunk->next = slab->first_partial_chunk;
        chunk->previous = NULL;
        if (slab->first_partial_chunk != NULL) {
            slab->first_partial_chunk->previous = chunk;
        }
        slab->first_partial_chunk = chunk;        
    }
    if (chunk->bitmap == ~ (BitmapType) 0U) {
        assert(was_full == 0);
        if (chunk->previous != NULL) {
            chunk->previous->next = chunk->next;
        }
        if (chunk->next != NULL) {
            chunk->next->previous = chunk->previous;
        }
        if (slab->first_partial_chunk == chunk) {
            slab->first_partial_chunk = slab->first_partial_chunk->next;
        }
        chunk->next = NULL;
        chunk->previous = NULL;        
        chunk->bitmap = (BitmapType) 0U;
        free(chunk);
    }
    return 0;
}

static int free_slab_entry_cb(void *context, void *entry,
                              const size_t sizeof_entry)
{
    FreeSlabEntryCB free_cb = context;

    (void) sizeof_entry;
    assert(entry != NULL);
    free_cb(entry);
    
    return 0;
}

void free_slab(Slab * const slab, FreeSlabEntryCB free_cb)
{
    if (slab == NULL) {
        return;
    }
    if (free_cb != NULL) {
        slab_foreach(slab, free_slab_entry_cb, free_cb);
    }
    SlabChunk *chunks_lists[] = {
        slab->first_partial_chunk, slab->first_full_chunk
    };
    SlabChunk * *chunk_list = chunks_lists;
    SlabChunk *chunk, *chunk_next;
    while (chunk_list != &chunks_lists
           [sizeof chunks_lists / sizeof chunks_lists[0]]) {
        chunk = *chunk_list++;
        while (chunk != NULL) {
            chunk_next = chunk->next;
            chunk->next = NULL;
            free(chunk);
            chunk = chunk_next;
        }
    }
    slab->first_partial_chunk = NULL;
    slab->first_full_chunk = NULL;
    slab->nb_slots_per_chunk = (size_t) 0U;
    slab->sizeof_entry = (size_t) 0U;
    slab->sizeof_slab_slot = (size_t) 0U;   
}

int slab_foreach(Slab * const slab, SlabForeachCB cb, void * const context)
{
    BitmapType bitmap;
    SlabSlot *slot;
    SlabChunk *chunk;
    unsigned int bits;
    BitmapType bit;
    unsigned char *slot_data;
    SlabChunk *chunks_lists[] = {
        slab->first_partial_chunk, slab->first_full_chunk
    };
    SlabChunk * *chunk_list = chunks_lists;
    int ret = 0;
    
    while (chunk_list != &chunks_lists
           [sizeof chunks_lists / sizeof chunks_lists[0]]) {
        chunk = *chunk_list++;
        while (chunk != NULL) {
            bitmap = chunk->bitmap;
            slot = chunk->unaligned_slots;            
            bits = slab->nb_slots_per_chunk;
            do {
                bit = bitmap & 1;
                bitmap >>= 1;
                if (bit != (BitmapType) 0U) {
                    goto next_slot;
                }
                slot_data = slot->unaligned_data +
                    slab->alignment_gap_before_data;
                if ((ret = cb(context, slot_data, slab->sizeof_entry)) != 0) {
                    return ret;
                }
                next_slot:
                slot = (SlabSlot *)
                    ((unsigned char *) slot + slab->sizeof_slab_slot);
            } while (--bits > 0U);
            chunk = chunk->next;
        }
    }
    return 0;
}
