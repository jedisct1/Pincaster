
#ifndef __SLAB_P_H__
#define __SLAB_P_H__ 1

typedef unsigned long BitmapType;

typedef struct SlabSlotFodder_ {
    struct SlabSlotFodder_ *fodder1;
    struct SlabSlotFodder_ *fodder2;
} SlabSlotFodder;

typedef struct SlabSlotFodderFirst_ {
    BitmapType bitmap;    
    struct SlabSlotFodder_ *fodder;
} SlabSlotFodderFirst;

typedef struct SlabSlot_ {
    struct SlabChunk_ *slab_chunk;
    unsigned char unaligned_data[];
} SlabSlot;

typedef struct SlabChunk_ {
    BitmapType bitmap;
    struct SlabChunk_ *next;
    struct SlabChunk_ *previous;
    SlabSlot unaligned_slots[];
} SlabChunk;

#endif
