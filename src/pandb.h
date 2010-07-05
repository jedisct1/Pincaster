
#ifndef __PANDB_H__
#define __PANDB_H__ 1

typedef float Dimension;
typedef float Dimension2;
typedef Dimension Meters;
typedef unsigned int NbSlots;
typedef unsigned long SubSlots;

#ifndef EARTH_CIRCUMFERENCE
# define EARTH_CIRCUMFERENCE (40000.0F * 1000.0F)
#endif
#ifndef EARTH_RADIUS
# define EARTH_RADIUS        (6371.0F * 1000.0F)
#endif
#ifndef DEG_AVG_DISTANCE
# define DEG_AVG_DISTANCE    (EARTH_CIRCUMFERENCE / 360.0F)
#endif
#ifndef DEFAULT_STACK_SIZE_FOR_SEARCHES
# define DEFAULT_STACK_SIZE_FOR_SEARCHES ((size_t) 8U)
#endif

typedef struct Position2D_ {
    Dimension latitude;    
    Dimension longitude;
} Position2D;

typedef struct Rectangle2D_ {
    Position2D edge0;
    Position2D edge1;   
} Rectangle2D;

typedef struct Slot_ {
    Position2D position;
#if PROJECTION
    Position2D real_position;
#endif
    struct KeyNode_    *key_node;
    struct BucketNode_ *bucket_node;
} Slot;

typedef struct Bucket_ {
    Slab slab;
    NbSlots bucket_size;
    NbSlots busy_slots;
} Bucket;

typedef enum NodeType_ {
    NODE_TYPE_NONE, NODE_TYPE_BARE_NODE, NODE_TYPE_QUAD_NODE,
        NODE_TYPE_BUCKET_NODE
} NodeType;

typedef enum LayerType_ {
    LAYER_TYPE_NONE, LAYER_TYPE_FLAT, LAYER_TYPE_FLATWRAP,
        LAYER_TYPE_SPHERICAL, LAYER_TYPE_GEOIDAL
} LayerType;

typedef struct BareNode_ {
    NodeType type;
    struct QuadNode_ *parent;
} BareNode;

typedef struct BucketNode_ {
    NodeType type;
    struct QuadNode_ *parent;    
    Bucket bucket;
} BucketNode;

typedef struct QuadNode_ {
    NodeType type;
    struct QuadNode_ *parent;
    SubSlots sub_slots;
    union Node_ *nodes[4];
} QuadNode;

typedef union Node_ {
    struct BareNode_   bare_node;
    struct BucketNode_ bucket_node;
    struct QuadNode_   quad_node;
} Node;

typedef struct Expirable_ {
    RB_ENTRY(Expirable_) entry;
    time_t ts;
    struct KeyNode_ *key_node;
} Expirable;

typedef RB_HEAD(Expirables_, Expirable_) Expirables;

typedef struct KeyNode_ {
    RB_ENTRY(KeyNode_) entry;
    Key *key;
    Slot *slot;
    SlipMap *properties;
    Expirable *expirable;
} KeyNode;

typedef RB_HEAD(KeyNodes_, KeyNode_) KeyNodes;

typedef enum Accuracy_ {
    ACCURACY_NONE, ACCURACY_HS, ACCURACY_GC, ACCURACY_FAST, ACCURACY_RHOMBOID
} Accuracy;

typedef struct PanDB_ {
    struct HttpHandlerContext_ *context;    
    QuadNode root;
    KeyNodes key_nodes;
    pthread_rwlock_t rwlock_db;
    Rectangle2D qbounds;
    Dimension latitude_accuracy;
    Dimension longitude_accuracy;
    LayerType layer_type;
    Accuracy accuracy;
    Expirables expirables;
} PanDB;

typedef struct QuadNodeWithBounds_ {
    const QuadNode *quad_node;
    Rectangle2D qrect;
} QuadNodeWithBounds;

typedef int (*FindNearCB)(void * const context,
                          Slot * const slot, Meters distance);

typedef int (*FindInRectCB)(void * const context,
                            Slot * const slot, Meters distance);

typedef int (*FindInRectClusterCB)(void * const context,
                                   const Position2D * const position,
                                   const Meters radius,
                                   const NbSlots children);

void dump(Node *scanned_node);

int init_pan_db(PanDB * const db,
                struct HttpHandlerContext_ * const context);

void free_pan_db(PanDB * const db);

int remove_entry_from_key_node(PanDB * const db,
                               KeyNode * const key_node,
                               const _Bool should_free_key_node);

int init_slot(Slot * const slot);

int add_slot(PanDB * const db, const Slot * const slot,
             Slot * * const new_slot);

int find_near(const PanDB * const db,
              FindNearCB cb, void * const cb_context,
              const Position2D * const position, const Meters distance,
              const SubSlots limit);

int find_in_rect(const PanDB * const db,
                 FindInRectCB cb, FindInRectClusterCB cluster_cb,
                 void * const cb_context,
                 const Rectangle2D * const rect,
                 const SubSlots limit, const Dimension epsilon);

#ifdef DEBUG
void print_rect(const Rectangle2D * const rect);
void print_position(const Position2D * const position);
void print_quad_rects(const Rectangle2D * const rects);
void dump_pan_db(Node *scanned_node);
void dump_keys(PanDB * const db);
#endif

#endif
