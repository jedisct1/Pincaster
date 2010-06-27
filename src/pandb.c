
#include "common.h"
#include "pandb.h"

static void get_qrects_from_qbounds(Rectangle2D qrects[4],
                                    const Rectangle2D * const qbounds)
{
    const Dimension median_latitude =
        (qbounds->edge0.latitude + qbounds->edge1.latitude) / 2.0;
    const Dimension median_longitude =
        (qbounds->edge0.longitude + qbounds->edge1.longitude) / 2.0;
    
    qrects[0].edge0.latitude = qbounds->edge0.latitude;
    qrects[0].edge0.longitude = median_longitude;
    qrects[0].edge1.latitude = median_latitude;
    qrects[0].edge1.longitude = qbounds->edge1.longitude;
    
    qrects[1].edge0.latitude = median_latitude;
    qrects[1].edge0.longitude = median_longitude;
    qrects[1].edge1 = qbounds->edge1;
    
    qrects[2].edge0 = qbounds->edge0;
    qrects[2].edge1.latitude = median_latitude;
    qrects[2].edge1.longitude = median_longitude;
    
    qrects[3].edge0.latitude = median_latitude;
    qrects[3].edge0.longitude = qbounds->edge0.longitude;
    qrects[3].edge1.latitude = qbounds->edge1.latitude;
    qrects[3].edge1.longitude = median_longitude;    
}

static int position_is_in_rect(const Position2D * const position,
                               const Rectangle2D * const rect)
{
    if (position->latitude >= rect->edge0.latitude &&
        position->longitude >= rect->edge0.longitude &&
        position->latitude < rect->edge1.latitude &&
        position->longitude < rect->edge1.longitude) {
        return 1;
    }
    return 0;
}

static Node *find_node_for_position(const QuadNode * const quad_node,
                                    const Rectangle2D * const qrects,
                                    const Position2D * const position,
                                    Rectangle2D * const rect_pnt,
                                    unsigned int *part_id)
{
    unsigned int t = 4U;
    
    assert(quad_node->type == NODE_TYPE_QUAD_NODE);
    do {
        t--;
        if (position_is_in_rect(position, &qrects[t])) {
            if (rect_pnt != NULL) {
                *rect_pnt = qrects[t];
            }
            if (part_id != NULL) {
                *part_id = t;
            }
            return quad_node->nodes[t];
        }
    } while (t > 0U);
#ifdef DEBUG
    print_position(position);    
    print_quad_rects(qrects);
#endif
    assert(0);
    
    return NULL;
}

int init_slot(Slot * const slot)
{
    slot->key_node = NULL;
    slot->bucket_node = NULL;
    
    return 0;
}

void free_slot(Slot * const slot)
{
    if (slot == NULL) {
        return;
    }
    assert(slot->key_node != NULL);
    slot->key_node = NULL;
    assert(slot->bucket_node != NULL);
    slot->bucket_node = NULL;
}

static int init_bucket(Bucket * const bucket)
{
    init_slab(&bucket->slab, sizeof(Slot), "slots");
    bucket->bucket_size = (NbSlots) BUCKET_SIZE;
    bucket->busy_slots = (NbSlots) 0U;
    
    return 0;    
}

static int free_bucket_cb(void *context, void *entry,
                          const size_t sizeof_entry)
{
    Slot *slot = entry;
    
    (void) context;
    (void) sizeof_entry;
    
    free_slot(slot);
    
    return 0;
}

static void free_bucket(Bucket * const bucket)
{
    if (bucket == NULL) {
        return;
    }
    slab_foreach((Slab *) &bucket->slab, free_bucket_cb, NULL);
    free_slab(&bucket->slab, NULL);
    bucket->bucket_size = (NbSlots) 0U;
    bucket->busy_slots = (NbSlots) 0U;    
}

static int init_bucket_node(BucketNode * const bucket_node,
                            const QuadNode * const parent)
{
    bucket_node->type = NODE_TYPE_BUCKET_NODE;
    assert(parent->type == NODE_TYPE_QUAD_NODE);
    bucket_node->parent = (QuadNode *) parent;
    init_bucket(&bucket_node->bucket);
    
    return 0;
}

static void free_bucket_node(BucketNode * const bucket_node)
{
    if (bucket_node == NULL) {
        return;
    }
    free_bucket(&bucket_node->bucket);
    bucket_node->type = NODE_TYPE_NONE;
    bucket_node->parent = NULL;
    free(bucket_node);
}

static BucketNode *new_bucket_node(const QuadNode * const parent)
{
    BucketNode *bucket_node;

    assert(parent->type == NODE_TYPE_QUAD_NODE);
    bucket_node = malloc(sizeof *bucket_node);
    if (bucket_node == NULL) {
        return NULL;
    }
    init_bucket_node(bucket_node, parent);

    return bucket_node;
}

static int init_quad_node(QuadNode * const quad_node)
{
    quad_node->type = NODE_TYPE_QUAD_NODE;
    quad_node->parent = NULL;
    quad_node->sub_slots = (SubSlots) 0U;
    quad_node->nodes[0] = (Node *) new_bucket_node(quad_node);    
    quad_node->nodes[1] = (Node *) new_bucket_node(quad_node);
    quad_node->nodes[2] = (Node *) new_bucket_node(quad_node);
    quad_node->nodes[3] = (Node *) new_bucket_node(quad_node);
    
    return 0;
}

static void free_quad_node(QuadNode * const quad_node)
{
    if (quad_node == NULL) {
        return;
    }
    assert(quad_node->type == NODE_TYPE_QUAD_NODE);
    quad_node->type = NODE_TYPE_NONE;
    quad_node->parent = NULL;    
    quad_node->nodes[0] = NULL;
    quad_node->nodes[1] = NULL;
    quad_node->nodes[2] = NULL;
    quad_node->nodes[3] = NULL;
    free(quad_node);
}

static QuadNode *new_quad_node(void)
{
    QuadNode *quad_node;
    
    quad_node = malloc(sizeof *quad_node);
    if (quad_node == NULL) {
        return NULL;
    }
    init_quad_node(quad_node);

    return quad_node;
}

static int add_slot_to_bucket(PanDB * const db, BucketNode * const bucket_node,
                              const Slot * const slot, int update_sub_slots,
                              Slot * * const new_slot)
{
    QuadNode *parent;
    Bucket *bucket;
    
    (void) db;
    *new_slot = NULL;
    assert(bucket_node->type == NODE_TYPE_BUCKET_NODE);
    bucket = &bucket_node->bucket;
    *new_slot = add_entry_to_slab(&bucket->slab, slot);
    if (*new_slot == NULL) {
        return -1;
    }
    assert(slot->key_node != NULL);
    * *new_slot = *slot;
    assert(bucket_node != NULL);
    (*new_slot)->bucket_node = bucket_node;
    bucket->busy_slots++;
    if (update_sub_slots == 1) {
        parent = bucket_node->parent;
        while (parent != NULL) {
            parent->sub_slots++;
            parent = parent->parent;
        }
    } else if (update_sub_slots == 2) {
        bucket_node->parent->sub_slots++;
    }
    return 0;
}

typedef struct RebalanceBucketCBContext_ {
    PanDB *db;
    QuadNode *quad_node_;
    Rectangle2D *qrects_;
} RebalanceBucketCBContext;

static int rebalance_bucket_cb(void *context_, void *entry,
                               const size_t sizeof_entry)
{
    Node * target_node;    
    Slot * scanned_slot = (Slot *) entry;
    BucketNode * target_bucket;
    RebalanceBucketCBContext *context = context_;
    Slot *new_slot;
    
    (void) sizeof_entry;
    target_node = find_node_for_position
        (context->quad_node_, context->qrects_,
         &scanned_slot->position, NULL, NULL);
    target_bucket = &target_node->bucket_node;    
    add_slot_to_bucket(context->db, target_bucket, scanned_slot, 2, &new_slot);
    scanned_slot->key_node->slot = new_slot;
    
    return 0;
}

int add_slot(PanDB * const db, const Slot * const slot,
             Slot * * const new_slot)
{
    Rectangle2D qrects[4];
    Rectangle2D qrect;
    QuadNode *scanned_node;
    Node *scanned_node_child;
    Rectangle2D qbounds = db->qbounds;
    KeyNode *key_node;
    unsigned int part_id;
    
    *new_slot = NULL;
    scanned_node = &db->root;
    key_node = slot->key_node;
    assert(key_node != NULL);
    
    rescan:
    get_qrects_from_qbounds(qrects, &qbounds);        
    scanned_node_child = find_node_for_position(scanned_node, qrects,
                                                &slot->position,
                                                &qrect, &part_id);    
    assert(((BareNode *) scanned_node_child)->parent == scanned_node);
    if (scanned_node_child->bare_node.type == NODE_TYPE_BUCKET_NODE) {        
        BucketNode * bucket_node = &scanned_node_child->bucket_node;        
        
        assert(bucket_node->bucket.bucket_size > (NbSlots) 0);
        if (bucket_node->bucket.busy_slots < bucket_node->bucket.bucket_size ||
            qrect.edge1.latitude - qrect.edge0.latitude <
            db->latitude_accuracy ||
            qrect.edge1.longitude - qrect.edge0.longitude <
            db->longitude_accuracy) {
            add_slot_to_bucket(db, bucket_node, slot, 1, new_slot);
            key_node->slot = *new_slot;
        } else {
            QuadNode *quad_node_;
            Rectangle2D qrects_[4];
            Node * target_node;
            BucketNode * target_bucket;
            Bucket *bucket;
            
            quad_node_ = new_quad_node();            
            quad_node_->parent = scanned_node;
            assert(quad_node_->parent->type == NODE_TYPE_QUAD_NODE);
            get_qrects_from_qbounds(qrects_, &qrect);            
            bucket = &bucket_node->bucket;
            assert(bucket != NULL);
            
            RebalanceBucketCBContext context = {
                    .db = db,
                    .quad_node_ = quad_node_,
                    .qrects_ = qrects_
            };
            slab_foreach(&bucket->slab, rebalance_bucket_cb, &context);
            free_bucket_node(&scanned_node->nodes[part_id]->bucket_node);
            scanned_node->nodes[part_id] = (Node *) quad_node_;

            target_node = find_node_for_position
                (quad_node_, qrects_, &slot->position, &qrect, &part_id);
            target_bucket = &target_node->bucket_node;
            add_slot_to_bucket(db, target_bucket, slot, 1, new_slot);
            key_node->slot = *new_slot;
        }
    } else if (scanned_node_child->bare_node.type == NODE_TYPE_QUAD_NODE) {
        scanned_node = &scanned_node_child->quad_node;
        qbounds = qrects[part_id];
        goto rescan;
    } else {
        assert(0);
    }    
    return 0;
}

typedef struct PackOldChildNodeCB_ {
    PanDB *db;
    BucketNode *new_node;
} PackOldChildNodeCB;

static int pack_old_child_node_cb(void *context_, void *entry,
                                  const size_t sizeof_entry)
{
    PackOldChildNodeCB *context = context_;
    Slot *slot = entry;
    Slot *new_slot;
    
    (void) sizeof_entry;
    add_slot_to_bucket(context->db, context->new_node,
                       slot, 0, &new_slot);
    slot->key_node->slot = new_slot;
    
    return 0;
}

int remove_entry_from_key_node(PanDB * const db,
                               KeyNode * const key_node,
                               const _Bool should_free_key_node)
{
    Slot * const slot = key_node->slot;
    BucketNode *bucket_node;
    QuadNode *scanned_node;
    Bucket *bucket;

    assert(slot != NULL);
    assert(key_node->key != NULL);
    if (should_free_key_node != 0) {
        RB_REMOVE(KeyNodes_, &db->key_nodes, key_node);        
        key_node->slot = NULL;
        free_key_node(db, key_node);
    }
    bucket_node = slot->bucket_node;
    assert(bucket_node != NULL);
    assert(bucket_node->type == NODE_TYPE_BUCKET_NODE);
    bucket = &bucket_node->bucket;
    free_slot(slot);    
    remove_entry_from_slab(&bucket->slab, slot);
    
    assert(bucket->busy_slots > (NbSlots) 0U);
    if (bucket->busy_slots > (NbSlots) 0U) {
        bucket->busy_slots--;
    }
    if (bucket_node->parent->parent != NULL &&
        bucket->busy_slots <= bucket->bucket_size / (NbSlots) 2U) {
        NbSlots busy_slots_in_siblings = (NbSlots) 0U;
        _Bool only_buckets = 1;
        unsigned int t;
        
        scanned_node = bucket_node->parent;
        assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
        t = 3U;
        do {
            if (scanned_node->nodes[t]->bare_node.type !=
                NODE_TYPE_BUCKET_NODE) {
                only_buckets = 0;
                break;
            }
            busy_slots_in_siblings +=
                scanned_node->nodes[t]->bucket_node.bucket.busy_slots;
        } while (t-- != 0U);
        if (only_buckets != 0 && busy_slots_in_siblings <
            bucket->bucket_size / (NbSlots) 4U * (NbSlots) 3U) {
            BucketNode *old_child_node;
            BucketNode *new_node;
            PackOldChildNodeCB context;
            
            new_node = new_bucket_node(scanned_node->parent);
            context = (PackOldChildNodeCB) {
                .db = db,
                .new_node = new_node
            };
            t = 3U;
            do {
                old_child_node = &scanned_node->nodes[t]->bucket_node;
                slab_foreach((Slab *) &old_child_node->bucket.slab,
                             pack_old_child_node_cb, &context);
            } while (t-- != 0U);
            assert(new_node->bucket.busy_slots == busy_slots_in_siblings);
            t = 3U;
            do {
                if ((QuadNode *) scanned_node->parent->nodes[t] == scanned_node) {
                    scanned_node->parent->nodes[t] = (Node *) new_node;
                    break;
                }
            } while (t-- != 0U);
            assert(new_node->parent == scanned_node->parent);
            assert(t < 4U);
            t = 3U;
            do {
                old_child_node = &scanned_node->nodes[t]->bucket_node;
                free_bucket_node(old_child_node);
            } while (t-- != 0U);
            free_quad_node(scanned_node);
            scanned_node = NULL;
            bucket_node = new_node;
        }
    }
    scanned_node = bucket_node->parent;
    assert(scanned_node != NULL);
    do {
        assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
        assert(scanned_node->sub_slots > (SubSlots) 0U);
        if (scanned_node->sub_slots > (SubSlots) 0U) {
            scanned_node->sub_slots--;   
        }
        scanned_node = scanned_node->parent;
    } while (scanned_node != NULL);
    
    return 0;
}

int remove_entry(PanDB * const db, Key * const key)
{
    KeyNode *key_node;
    KeyNode scanned_key_node;    
    
    scanned_key_node.key = key;    
    key_node = RB_FIND(KeyNodes_, &db->key_nodes, &scanned_key_node);
    if (key_node != NULL) {
        assert(key_node->slot != NULL);
        assert(key_node->slot->bucket_node != NULL);
        assert(key_node->slot->bucket_node->type == NODE_TYPE_BUCKET_NODE);
        remove_entry_from_key_node(db, key_node, 1);
        return 1;
    }
    return 0;
}

static int rectangle2d_intersect(const Rectangle2D * const r1,
                                 const Rectangle2D * const r2)
{
    if (!(r1->edge0.longitude > r2->edge1.longitude ||
          r1->edge1.longitude < r2->edge0.longitude ||
          r1->edge0.latitude > r2->edge1.latitude ||
          r1->edge1.latitude < r2->edge0.latitude)) {        
        return 1;
    }
    return 0;
}

typedef struct FindNearIntCBContext_ {    
    const PanDB *db;
    const Position2D *position;
    Meters distance;
    SubSlots limit;
    FindNearCB cb;
    void *context_cb;
} FindNearIntCBContext;

static int find_near_context_cb(void *context_, void *entry,
                                const size_t sizeof_entry)
{
    FindNearIntCBContext *context = context_;
    Slot *scanned_slot = entry;
    Meters cd;
    
    (void) sizeof_entry;
    if (context->db->layer_type == LAYER_TYPE_SPHERICAL ||
        context->db->layer_type == LAYER_TYPE_GEOIDAL) {
        switch (context->db->accuracy) {
        case ACCURACY_HS:
            cd = hs_distance_between_geoidal_positions
                (context->position, &scanned_slot->position);
            break;
        case ACCURACY_GC:
            cd = gc_distance_between_geoidal_positions
                (context->position, &scanned_slot->position);            
            break;
        case ACCURACY_FAST:
            cd = gc_distance_between_geoidal_positions
                (context->position, &scanned_slot->position);            
            break;
        case ACCURACY_RHOMBOID:
            cd = rhomboid_distance_between_geoidal_positions
                (context->position, &scanned_slot->position);
            break;
        default:
            assert(0);
            return -1;
        }
    } else {
        cd = distance_between_flat_positions(context->db,
                                             context->position,
                                             &scanned_slot->position);
    }
    if (cd <= context->distance) {
       if (scanned_slot->key_node != NULL) {
           if (context->cb != NULL) {               
               const int ret =
                   context->cb(context->context_cb, scanned_slot, cd);
               if (ret != 0) {
                   return ret;
               }
               if (context->limit-- <= (SubSlots) 1U) {
                   return 1;
               }
           }
           return 0;
       } else {
           printf("=== [NULL KEY] => %.3f/%.3f (%.2f)\n",
                  scanned_slot->position.latitude,
                  scanned_slot->position.longitude,
                  cd);
       }
    }
    return 0;
}

static int find_near_in_zone(Rectangle2D * const matching_rect,
                             PntStack *stack_inspect,
                             const PanDB * const db,
                             FindNearCB cb,
                             void * const context_cb,
                             const Position2D * const position,
                             const Meters distance,
                             const SubSlots limit)
{
    const QuadNode *scanned_node;
    Rectangle2D scanned_qbounds = db->qbounds;
    Rectangle2D scanned_children_qbounds[4];
    unsigned int t;
    Node *scanned_node_child;
    Rectangle2D *scanned_child_qbound;
    QuadNodeWithBounds qnb;
    QuadNodeWithBounds *sqnb;    
    
    scanned_node = &db->root;    
    FindNearIntCBContext context = {
        .db = db,
        .position = position,
        .distance = distance,
        .cb = cb,
        .context_cb = context_cb,
        .limit = limit
    };
    for (;;) {
        assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
        get_qrects_from_qbounds(scanned_children_qbounds, &scanned_qbounds);
        t = 0U;
        do {
            scanned_node_child = scanned_node->nodes[t];
            scanned_child_qbound = &scanned_children_qbounds[t];
            if (rectangle2d_intersect(scanned_child_qbound,
                                      matching_rect) == 0) {
                continue;
            }
            if (scanned_node_child->bare_node.type == NODE_TYPE_BUCKET_NODE) {
                const Bucket *bucket = &scanned_node_child->bucket_node.bucket;
                const int ret = slab_foreach((Slab *) &bucket->slab,
                                             find_near_context_cb, &context);
                if (ret != 0) {
                    return ret;
                }
                continue;
            }
            assert(scanned_node_child->bare_node.type == NODE_TYPE_QUAD_NODE);
            qnb.quad_node = &scanned_node_child->quad_node;
            qnb.qrect = *scanned_child_qbound;
            push_pnt_stack(stack_inspect, &qnb);
        } while (t++ < 3U);
        sqnb = pop_pnt_stack(stack_inspect);
        if (sqnb == NULL) {
            break;
        }
        scanned_node = sqnb->quad_node;
        scanned_qbounds = sqnb->qrect;
    }
    return 0;
}

static inline Dimension dimension_min(const Dimension d1,
                                      const Dimension d2)
{
    return d1 < d2 ? d1 : d2;
}

static inline Dimension dimension_max(const Dimension d1,
                                      const Dimension d2)
{
    return d1 > d2 ? d1 : d2;    
}

static unsigned int find_zones(const PanDB * const db,
                               const Rectangle2D * const rect_,
                               Rectangle2D rects[4])
{
    const Rectangle2D qbounds = db->qbounds;
    Rectangle2D *rect_pnt = &rects[0];    
    Rectangle2D rect = (Rectangle2D) {
        .edge0 = {
            .latitude = dimension_max(qbounds.edge0.latitude * 2.0,
                                      rect_->edge0.latitude),
            .longitude = dimension_max(qbounds.edge0.longitude * 2.0,
                                       rect_->edge0.longitude)
        },
        .edge1 = {
            .latitude = dimension_min(qbounds.edge1.latitude * 2.0,
                                      rect_->edge1.latitude),
            .longitude = dimension_min(qbounds.edge1.longitude * 2.0,
                                       rect_->edge1.longitude)
        }        
    };
    const Rectangle2D bounded_rect = {
        .edge0 = {
            .latitude = dimension_max(qbounds.edge0.latitude,
                                      rect.edge0.latitude),
            .longitude = dimension_max(qbounds.edge0.longitude,
                                       rect.edge0.longitude)
        },
        .edge1 = {
            .latitude = dimension_min(qbounds.edge1.latitude,
                                      rect.edge1.latitude),
            .longitude = dimension_min(qbounds.edge1.longitude,
                                       rect.edge1.longitude)
        }
    };
    *rect_pnt++ = bounded_rect;
    if (rect.edge0.latitude < qbounds.edge0.latitude) {
        *rect_pnt++ = (Rectangle2D) {
            {
                dimension_max(bounded_rect.edge1.latitude,
                              rect.edge0.latitude - qbounds.edge0.latitude +
                              qbounds.edge1.latitude),
                bounded_rect.edge0.longitude
            },
            {
                qbounds.edge1.latitude,
                bounded_rect.edge1.longitude 
            }
        };
    }
    if (rect.edge0.longitude < qbounds.edge0.longitude) {
        *rect_pnt++ = (Rectangle2D) {
            {
                bounded_rect.edge0.latitude,
                dimension_max(bounded_rect.edge1.longitude,
                              rect.edge0.longitude - qbounds.edge0.longitude +
                              qbounds.edge1.longitude)
            },
            {
                bounded_rect.edge1.latitude,
                qbounds.edge1.longitude
            }
        };
    }
    if (rect.edge1.latitude > qbounds.edge1.latitude) {
        *rect_pnt++ = (Rectangle2D) {
            { 
                qbounds.edge0.latitude,
                bounded_rect.edge0.longitude
            },
            { 
                dimension_min(bounded_rect.edge0.latitude,
                              rect.edge1.latitude + qbounds.edge0.latitude -
                              qbounds.edge1.latitude),
                bounded_rect.edge1.longitude
            }
        };
    }
    if (rect.edge1.longitude > qbounds.edge1.longitude) {
        if ((unsigned int) (rect_pnt - &rects[0]) >= 4U) {
            return 1U;
        }
        *rect_pnt++ = (Rectangle2D) {
            {
                bounded_rect.edge0.latitude,
                qbounds.edge0.longitude,
            },
            {
                bounded_rect.edge1.latitude,
                dimension_min(bounded_rect.edge0.longitude,
                              rect.edge1.longitude + qbounds.edge0.longitude -
                              qbounds.edge1.longitude)
            }
        };
    }
    return (unsigned int) (rect_pnt - &rects[0]);
}

int find_near(const PanDB * const db,
              FindNearCB cb, void * const context_cb,
              const Position2D * const position, const Meters distance,
              const SubSlots limit)
{
    if (limit <= (SubSlots) 0) {
        return 0;
    }
    Rectangle2D matching_rects[4];
    Rectangle2D *matching_rect = &matching_rects[0];    
    PntStack *stack_inspect;

    const Dimension dlat = distance / DEG_AVG_DISTANCE;
    const Dimension dlon = distance /
        fabs(cosf((float) DEG_TO_RAD(position->latitude)) * DEG_AVG_DISTANCE);
    const Rectangle2D rect = { {
        position->latitude - dlat, position->longitude - dlon
    }, {
        position->latitude + dlat, position->longitude + dlon
    } };
    unsigned int nb_zones;
    if (db->layer_type == LAYER_TYPE_FLAT) {
        matching_rects[0] = rect;        
        nb_zones = 1U;
    } else {
        nb_zones = find_zones(db, &rect, matching_rects);
    }
    assert(nb_zones >= 1U);
    assert(nb_zones <= 4U);
    stack_inspect = new_pnt_stack(DEFAULT_STACK_SIZE_FOR_SEARCHES,
                                  sizeof(QuadNodeWithBounds));
    if (stack_inspect == NULL) {
        return -1;
    }
    assert(matching_rect == &matching_rects[0]);
    int ret;
    do {
        ret = find_near_in_zone(matching_rect,
                                stack_inspect,
                                db,
                                cb,
                                context_cb,
                                position,
                                distance,
                                limit);
        matching_rect++;
    } while (ret == 0 && --nb_zones > 0U);
    free_pnt_stack(stack_inspect);
    
    return ret;
}

typedef struct FindInRectIntCBContext_ {    
    const PanDB *db;
    const Position2D *position;
    const Rectangle2D *rect;
    SubSlots limit;
    FindInRectCB cb;
    FindInRectClusterCB cluster_cb;
    void *context_cb;
} FindInRectIntCBContext;

static int find_in_rect_context_cb(void *context_, void *entry,
                                   const size_t sizeof_entry)
{
    FindInRectIntCBContext *context = context_;
    Slot *scanned_slot = entry;
    Meters cd;
    
    (void) sizeof_entry;
    if (position_is_in_rect(&scanned_slot->position, context->rect) == 0) {
        return 0;
    }
    if (context->db->layer_type == LAYER_TYPE_SPHERICAL ||
        context->db->layer_type == LAYER_TYPE_GEOIDAL) {
        cd = rhomboid_distance_between_geoidal_positions
            (context->position, &scanned_slot->position);
    } else {
        cd = distance_between_flat_positions(context->db,
                                             context->position,
                                             &scanned_slot->position);
    }
    if (scanned_slot->key_node != NULL) {
        if (context->limit <= (SubSlots) 0U) {
            return 1;
        }
        context->limit--;
        if (context->cb != NULL) {
            const int ret =
                context->cb(context->context_cb, scanned_slot, cd);
            if (ret != 0) {
                return ret;
            }
        }
    } else {
        fprintf(stderr, "=== [NULL KEY] => %.3f/%.3f (%.2f)\n",
                scanned_slot->position.latitude,
                scanned_slot->position.longitude,
                cd);
    }
    return 0;
}

static int find_in_rect_cluster_context_cb(void *context_, void *entry,
                                           const size_t sizeof_entry)
{
    FindInRectIntCBContext *context = context_;
    if (context->cluster_cb == NULL) {
        return 0;
    }
    QuadNode *scanned_node = entry;
    const Rectangle2D * const rect = context->rect;
    const Position2D center = {
        .latitude = (rect->edge1.latitude + rect->edge0.latitude) / 2.0,
        .longitude = (rect->edge1.longitude + rect->edge0.longitude) / 2.0
    };
    if (position_is_in_rect(&center, context->rect) == 0) {
        return 0;
    }    
    const Dimension distance =
        (fabsf(center.latitude - rect->edge0.latitude) +
            fabsf(center.longitude - rect->edge0.longitude)) / 2.0;
    Meters radius;
    if (context->db->layer_type == LAYER_TYPE_SPHERICAL ||
        context->db->layer_type == LAYER_TYPE_GEOIDAL) {
        radius = geoidal_distance_to_meters(distance);
    } else {
        radius = (Meters) distance;
    }
    assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
    const NbSlots children = scanned_node->sub_slots;
    (void) sizeof_entry;
    
    const int ret =
        context->cluster_cb(context->context_cb, &center, radius, children);
    if (ret != 0) {
        return ret;
    }
    if (context->limit-- <= (SubSlots) 1U) {
        return 1;
    }
    return 0;
}

static int find_in_rect_in_zone(Rectangle2D * const matching_rect,
                                PntStack *stack_inspect,
                                const PanDB * const db,
                                FindInRectCB cb,
                                FindInRectClusterCB cluster_cb,
                                void * const context_cb,
                                const Rectangle2D * const rect,
                                const SubSlots limit, const Dimension epsilon)
{
    const Position2D rect_center = {
        .latitude =
        (rect->edge1.latitude + rect->edge0.latitude) / (Dimension) 2.0,
        .longitude =
        (rect->edge1.longitude + rect->edge0.longitude) / (Dimension) 2.0
    };
    const _Bool cluster = (epsilon > (Dimension) 0.0);
    SubSlots max_nb_slots_without_clustering = (SubSlots) 0U;
    Rectangle2D scanned_qbounds = db->qbounds;
    const QuadNode *scanned_node;
    Rectangle2D scanned_children_qbounds[4];
    Rectangle2D *scanned_child_qbound;
    QuadNodeWithBounds qnb;
    QuadNodeWithBounds *sqnb;
    Node *scanned_node_child;
    unsigned int t;
    
    scanned_node = &db->root;
    FindInRectIntCBContext context = {
        .db = db,
        .cb = cb,
        .cluster_cb = cluster_cb,
        .context_cb = context_cb,
        .position = &rect_center,
        .limit = limit
    };
    for (;;) {
        assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
        get_qrects_from_qbounds(scanned_children_qbounds, &scanned_qbounds);
        if (cluster != 0) {
            max_nb_slots_without_clustering = context.limit / (SubSlots) 4U;
        }
        t = 0U;
        do {
            scanned_node_child = scanned_node->nodes[t];
            scanned_child_qbound = &scanned_children_qbounds[t];
            if (rectangle2d_intersect(scanned_child_qbound,
                                      matching_rect) == 0) {
                continue;
            }
            if (scanned_node_child->bare_node.type == NODE_TYPE_QUAD_NODE &&
                cluster != 0 &&
                scanned_node_child->quad_node.sub_slots >
                max_nb_slots_without_clustering &&
                fabsf(scanned_child_qbound->edge1.latitude -
                      scanned_child_qbound->edge0.latitude) < epsilon &&
                fabsf(scanned_child_qbound->edge1.longitude -
                      scanned_child_qbound->edge0.longitude) < epsilon) {
                context.rect = scanned_child_qbound;
                const int ret = find_in_rect_cluster_context_cb
                    (&context,
                        (void *) scanned_node_child, sizeof *scanned_node_child);
                if (ret != 0) {
                    return ret;
                }
                continue;
            }
            if (scanned_node_child->bare_node.type == NODE_TYPE_BUCKET_NODE) {
                const Bucket *bucket = &scanned_node_child->bucket_node.bucket;
                context.rect = rect;
                const int ret = slab_foreach((Slab *) &bucket->slab,
                                             find_in_rect_context_cb, &context);
                if (ret != 0) {
                    return ret;
                }
                continue;
            }
            assert(scanned_node_child->bare_node.type == NODE_TYPE_QUAD_NODE);
            qnb.quad_node = &scanned_node_child->quad_node;
            qnb.qrect = *scanned_child_qbound;
            push_pnt_stack(stack_inspect, &qnb);
        } while (t++ < 3U);
        sqnb = pop_pnt_stack(stack_inspect);
        if (sqnb == NULL) {
            break;
        }
        scanned_node = sqnb->quad_node;
        scanned_qbounds = sqnb->qrect;
    }
    return 0;
}

int find_in_rect(const PanDB * const db,
                 FindInRectCB cb, FindInRectClusterCB cluster_cb,
                 void * const context_cb,
                 const Rectangle2D * const rect,
                 const SubSlots limit, const Dimension epsilon)
{
    if (limit <= (SubSlots) 0) {
        return 0;
    }
    Rectangle2D matching_rects[4];
    Rectangle2D *matching_rect = &matching_rects[0];
    PntStack *stack_inspect;
    unsigned int nb_zones;
    
    if (db->layer_type == LAYER_TYPE_FLAT) {
        matching_rects[0] = *rect;
        nb_zones = 1U;
    } else {
        Rectangle2D orect = *rect;
        if (orect.edge0.latitude > orect.edge1.latitude) {
            orect.edge1.latitude +=
                db->qbounds.edge1.latitude - db->qbounds.edge0.latitude;
        }
        if (orect.edge0.longitude > orect.edge1.longitude) {
            orect.edge1.longitude +=
                db->qbounds.edge1.longitude - db->qbounds.edge0.longitude;
        }        
        nb_zones = find_zones(db, &orect, matching_rects);
    }
    assert(nb_zones >= 1U);
    assert(nb_zones <= 4U);
    
    stack_inspect = new_pnt_stack(DEFAULT_STACK_SIZE_FOR_SEARCHES,
                                  sizeof(QuadNodeWithBounds));
    if (stack_inspect == NULL) {
        return -1;
    }    
    assert(matching_rect == &matching_rects[0]);
    int ret;
    do {
        ret = find_in_rect_in_zone(matching_rect,
                                   stack_inspect,
                                   db,
                                   cb,
                                   cluster_cb,
                                   context_cb,
                                   matching_rect,
                                   limit,
                                   epsilon);
        matching_rect++;
    } while (ret == 0 && --nb_zones > 0U);    
    free_pnt_stack(stack_inspect);

    return ret;
}

int init_pan_db(PanDB * const db,
                struct HttpHandlerContext_ * const context)
{
    pthread_rwlock_init(&db->rwlock_db, NULL);
    init_quad_node(&db->root);
    db->qbounds = (Rectangle2D) {
        .edge0 = { .latitude = -180.0F, .longitude = -180.0F },
        .edge1 = { .latitude =  180.0F, .longitude =  180.0F }
    };
    db->latitude_accuracy = db->longitude_accuracy =
        app_context.dimension_accuracy;
    db->layer_type = app_context.default_layer_type;
    db->accuracy = app_context.default_accuracy;
    assert(context != NULL);
    db->context = context;
    RB_INIT(&db->key_nodes);
    RB_INIT(&db->expirables);    
    
    return 0;
}

void free_pan_db(PanDB * const db)
{
    unsigned int t;
    PntStack *stack_inspect;
    PntStack *stack_quad_nodes_to_delete;    
    QuadNode *scanned_node;
    QuadNode *qn;
    QuadNode * *sqn;
    Node *scanned_node_child;
    
    if (db == NULL) {
        return;
    }
    KeyNode *scanned_key_node;
    KeyNode *next_key_node;
    for (scanned_key_node = RB_MIN(KeyNodes_, &db->key_nodes);
         scanned_key_node != NULL; scanned_key_node = next_key_node) {
        next_key_node = RB_NEXT(KeyNodes_, &db->key_nodes,
                                scanned_key_node);
        RB_REMOVE(KeyNodes_, &db->key_nodes, scanned_key_node);
        scanned_key_node->slot = NULL;
        free_key_node(db, scanned_key_node);
    }
    stack_inspect = new_pnt_stack((size_t) 8U, sizeof qn);
    stack_quad_nodes_to_delete = new_pnt_stack((size_t) 8U, sizeof qn);
    scanned_node = &db->root;
    for (;;) {
        assert(scanned_node->type == NODE_TYPE_QUAD_NODE);
        t = 0U;
        do {
            scanned_node_child = scanned_node->nodes[t];
            if (scanned_node_child->bare_node.type == NODE_TYPE_BUCKET_NODE) {
                free_bucket_node(&scanned_node_child->bucket_node);
                assert(scanned_node_child == scanned_node->nodes[t]);
                scanned_node->nodes[t] = NULL;
                continue;
            }
            assert(scanned_node_child->bare_node.type == NODE_TYPE_QUAD_NODE);
            qn = &scanned_node_child->quad_node;
            push_pnt_stack(stack_inspect, &qn);
            push_pnt_stack(stack_quad_nodes_to_delete, &qn);
        } while (t++ < 3U);
        sqn = pop_pnt_stack(stack_inspect);
        if (sqn == NULL) {
            break;
        }
        scanned_node = *sqn;
    }
    free_pnt_stack(stack_inspect);
    while ((sqn = pop_pnt_stack(stack_quad_nodes_to_delete)) != NULL) {
        qn = *sqn;
        assert(qn->type == NODE_TYPE_QUAD_NODE);
        free_quad_node(qn);
    }
    free_pnt_stack(stack_quad_nodes_to_delete);
    assert(db->context != NULL);
    db->context = NULL;
    pthread_rwlock_destroy(&db->rwlock_db);
}

#ifdef DEBUG
void print_rect(const Rectangle2D * const rect)
{
    printf("rect = (%.3f, %.3f) - (%.3f, %.3f)\n",
           rect->edge0.latitude, rect->edge0.longitude,
           rect->edge1.latitude, rect->edge1.longitude);
}

void print_position(const Position2D * const position)
{
    printf("position = (%.3f, %.3f)\n",
           position->latitude, position->longitude);
}

void print_quad_rects(const Rectangle2D * const rects)
{
    puts("Quad:");
    putchar('\t');
    print_rect(&rects[0]);
    putchar('\t');    
    print_rect(&rects[1]);
    putchar('\t');
    print_rect(&rects[2]);
    putchar('\t');    
    print_rect(&rects[3]);    
}

static int dump_bucket_node_cb(void *context, void *entry,
                               const size_t sizeof_entry)
{
    _Bool *coma = context;
    Slot *scanned_slot = entry;

    (void) sizeof_entry;
    if (*coma != 0) {
        printf(", ");
    } else {
        *coma = 1;
    }
    const char *key_s;
    
    key_s = (const char *) scanned_slot->key_node->key->val;
    printf("\"%.3f, %.3f [%s](%p => %p)\"",
           scanned_slot->position.latitude,
           scanned_slot->position.longitude,
           key_s, scanned_slot, scanned_slot->bucket_node);
    assert(scanned_slot->bucket_node != NULL);
    
    return 0;
}

static void dump_bucket_node(const BucketNode * const bucket_node)
{
    const Bucket *bucket = &bucket_node->bucket;
    _Bool coma = 0;

    assert(bucket != NULL);
    printf("[");
    slab_foreach((Slab *) &bucket->slab, dump_bucket_node_cb, &coma);
    printf("]");    
}

static void dump_quad_node(const QuadNode * const quad_node)
{
    unsigned int t = 0U;
    
    printf("{");
    printf("\"sub_slots\":%lu, ", (unsigned long) quad_node->sub_slots);
    do {
        printf("\"%u\":", t);
        dump_pan_db(quad_node->nodes[t]);
        if (t < 3U) {
            printf(", ");
        }
    } while (t++ < 3U);
    printf("}\n");
    fflush(stdout);    
}

void dump_pan_db(Node *scanned_node)
{
    if (scanned_node->bare_node.type == NODE_TYPE_BUCKET_NODE) {
        dump_bucket_node(&scanned_node->bucket_node);
        return;
    }
    assert(scanned_node->bare_node.type == NODE_TYPE_QUAD_NODE);
    dump_quad_node(&scanned_node->quad_node);
}

void dump_keys(PanDB * const db)
{
    KeyNode *scanned_key_node;
    KeyNode *next_key_node;
    puts("\n\n--KEY--");
    for (scanned_key_node = RB_MIN(KeyNodes_, &db->key_nodes);
         scanned_key_node != NULL; scanned_key_node = next_key_node) {
        next_key_node = RB_NEXT(KeyNodes_, &db->key_nodes,
                                scanned_key_node);
        assert(scanned_key_node->key != NULL);
        assert(scanned_key_node->key->val != NULL);
        printf("KEY [%s] => (%p => %p)\n",
               scanned_key_node->key->val,
               scanned_key_node->slot,
               scanned_key_node->slot->bucket_node);
        assert(scanned_key_node->slot != NULL);
        assert(scanned_key_node->slot->bucket_node != NULL);
    }
    puts("--/KEY--\n");
}
#endif
