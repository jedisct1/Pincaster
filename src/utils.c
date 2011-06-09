
#include "common.h"
#include "key_nodes.h"
#include "utils.h"

void skip_spaces(const char * * const str)
{
    const char *s = *str;
    while (*s != 0 && isspace((unsigned char ) *s)) {
        s++;
    }
    *str = s;
}

typedef struct TraversedKeyNode_ {
    KeyNode * key_node;
    _Bool traversed;
} TraversedKeyNode;

typedef struct TraversalStackIsDuplicateCBContext_ {
    const void * const linked_key_s;
    const size_t linked_key_s_len;
    _Bool found;
} TraversalStackIsDuplicateCBContext;

static int traversal_stack_is_duplicate_cb(void * const context_,
                                           void * const traversed_key_node_)
{
    TraversalStackIsDuplicateCBContext * const context = context_;
    const TraversedKeyNode * const traversed_key_node = traversed_key_node_;
    const Key * const traversed_key = traversed_key_node->key_node->key;
    
    if (strcmp(context->linked_key_s, traversed_key->val) == 0) {
        assert(context->linked_key_s_len == traversed_key->len);
        context->found = 1;
        return 1;
    }    
    return 0;
}

typedef struct RecordsGetPropertiesCBContext_ {
    yajl_gen json_gen;
    PanDB * const pan_db;
    PntStack * const traversal_stack;
} RecordsGetPropertiesCBContext;

static int records_get_properties_cb(void * const context_,
                                     const void * const key,
                                     const size_t key_len,
                                     const void * const value,
                                     const size_t value_len)
{
    RecordsGetPropertiesCBContext * const context = context_;

    yajl_gen_string(context->json_gen, (const unsigned char *) key,
                    (unsigned int) key_len);    
    yajl_gen_string(context->json_gen, (const unsigned char *) value,
                    (unsigned int) value_len);
    if (context->traversal_stack == NULL ||
        key_len <= sizeof INT_PROPERTY_LINK - (size_t) 1U ||
        memcmp(key, INT_PROPERTY_LINK,
               sizeof INT_PROPERTY_LINK - (size_t) 1U) != 0) {
        return 0;
    }
    if (value_len <= (size_t) 0U || (* (const char *) value) == 0) {
        return 0;
    }
    int status;
    KeyNode *linked_key_node;
    Key * const linked_key = new_key_with_leading_zero(value, value_len);
    status = get_key_node_from_key(context->pan_db, linked_key, 0,
                                   &linked_key_node);
    if (status < 0) {
        release_key(linked_key);        
        return -1;
    }
    if (status == 0) {
        release_key(linked_key);        
        return 0;
    }
    TraversalStackIsDuplicateCBContext
        traversal_stack_is_duplicate_cb_context = {
            .linked_key_s = linked_key->val,
            .linked_key_s_len = linked_key->len,
            .found = 0
        };
    pnt_stack_foreach(context->traversal_stack,
                      traversal_stack_is_duplicate_cb,
                      &traversal_stack_is_duplicate_cb_context);
    release_key(linked_key);    
    if (traversal_stack_is_duplicate_cb_context.found != 0) {
        return 0;
    }
    TraversedKeyNode traversed_key_node = {
        .key_node = linked_key_node,
        .traversed = 0
    };
    if (push_pnt_stack(context->traversal_stack, &traversed_key_node) != 0) {
        return -1;
    }
    return 0;
}

static int key_node_to_json_(KeyNode * const key_node, yajl_gen json_gen,
                             PanDB * const pan_db,                             
                             const _Bool with_properties,
                             PntStack * const traversal_stack)
{
    (void) traversal_stack;
    
    yajl_gen_string(json_gen, (const unsigned char *) "key",
                    (unsigned int) sizeof "key" - (size_t) 1U);
    yajl_gen_string(json_gen, (const unsigned char *) key_node->key->val,
                    (unsigned int) strlen(key_node->key->val));
    yajl_gen_string(json_gen, (const unsigned char *) "type",
                    (unsigned int) sizeof "type" - (size_t) 1U);
    const char *type = "void";
    if (key_node->slot != NULL) {
        if (key_node->properties != NULL) {
            type = "point+hash";
        } else {
            type = "point";
        }
    } else if (key_node->properties != NULL) {
        type = "hash";
    }
    yajl_gen_string(json_gen, (const unsigned char *) type,
                    (unsigned int) strlen(type));
    if (key_node->slot != NULL) {
#if PROJECTION
        yajl_gen_string(json_gen, (const unsigned char *) "latitude",
                        (unsigned int) sizeof "latitude" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->real_position.latitude);
        yajl_gen_string(json_gen, (const unsigned char *) "longitude",
                        (unsigned int) sizeof "longitude" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->real_position.longitude);
        
        yajl_gen_string(json_gen, (const unsigned char *) "latitude_proj",
                        (unsigned int) sizeof "latitude_proj" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->position.latitude);
        yajl_gen_string(json_gen, (const unsigned char *) "longitude_proj",
                        (unsigned int) sizeof "longitude_proj" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->position.longitude);
#else
        yajl_gen_string(json_gen, (const unsigned char *) "latitude",
                        (unsigned int) sizeof "latitude" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->position.latitude);
        yajl_gen_string(json_gen, (const unsigned char *) "longitude",
                        (unsigned int) sizeof "longitude" - (size_t) 1U);
        yajl_gen_double(json_gen,
                        (double) key_node->slot->position.longitude);
#endif
    }
    if (with_properties == 0) {
        return 0;
    }
    if (key_node->expirable != NULL) {
        assert(key_node->expirable->ts != (time_t) 0);
        yajl_gen_string(json_gen, (const unsigned char *) "expires_at",
                        (unsigned int) sizeof "expires_at" - (size_t) 1U);
        yajl_gen_integer(json_gen, (long) key_node->expirable->ts);
    }
    RecordsGetPropertiesCBContext cb_context = {
        .json_gen = json_gen,
        .pan_db = pan_db,
        .traversal_stack = traversal_stack
    };
    if (key_node->properties != NULL) {
        yajl_gen_string(json_gen, (const unsigned char *) "properties",
                        (unsigned int) sizeof "properties" - (size_t) 1U);
        yajl_gen_map_open(json_gen);
        slip_map_foreach(&key_node->properties, records_get_properties_cb,
                         &cb_context);
        yajl_gen_map_close(json_gen);        
    }    
    return 0;
}

typedef struct TraverseKeysNodesForeachCBContext_ {
    yajl_gen json_gen;
    PanDB * const pan_db;
    _Bool with_properties;
    PntStack * const traversal_stack;
    _Bool found_job;
} TraverseKeysNodesForeachCBContext;

static int traverse_keys_nodes_foreach_cb(void * const context_,
                                          void * const traversed_key_node_)
{
    TraverseKeysNodesForeachCBContext * const context = context_;
    TraversedKeyNode * const traversed_key_node = traversed_key_node_;
    
    if (traversed_key_node->traversed == 1) {
        return 0;
    }
    const Key * const key = traversed_key_node->key_node->key;
    yajl_gen_string(context->json_gen,
                    (const unsigned char *) key->val,
                    (unsigned int) (key->len - (size_t) 1U));
    yajl_gen_map_open(context->json_gen);
    const int ret = key_node_to_json_(traversed_key_node->key_node,
                                      context->json_gen, context->pan_db,
                                      context->with_properties,
                                      context->traversal_stack);
    yajl_gen_map_close(context->json_gen);
    traversed_key_node->traversed = 1;
    context->found_job = 1;
    
    return ret;
}

int key_node_to_json(KeyNode * const key_node, yajl_gen json_gen,
                     PanDB * const pan_db,
                     const _Bool with_properties,
                     const _Bool with_links)
{
    if (with_links == 0) {    
        return key_node_to_json_(key_node, json_gen, pan_db,
                                 with_properties, NULL);
    }    
    PntStack * const traversal_stack =
        new_pnt_stack(INITIAL_TRAVERSAL_STACK_SIZE, sizeof(TraversedKeyNode));
    if (traversal_stack == NULL) {
        return -1;
    }
    TraversedKeyNode traversed_key_node = {
        .key_node = key_node,
        .traversed = 1
    };    
    if (push_pnt_stack(traversal_stack, &traversed_key_node) != 0) {
        free_pnt_stack(traversal_stack);
        return -1;
    }
    if (key_node_to_json_(key_node, json_gen, pan_db, with_properties,
                          traversal_stack) != 0) {
        free_pnt_stack(traversal_stack);
        return -1;
    }
    TraverseKeysNodesForeachCBContext context = {
        .json_gen = json_gen,
        .pan_db = pan_db,
        .with_properties = with_properties,
        .traversal_stack = traversal_stack
    };
    yajl_gen_string(json_gen, (const unsigned char *) "$links",
                    (unsigned int) sizeof "$links" - (size_t) 1U);
    yajl_gen_map_open(json_gen);
    do {
        context.found_job = 0;
        pnt_stack_foreach(traversal_stack, traverse_keys_nodes_foreach_cb,
                          &context);
    } while (context.found_job != 0);
    yajl_gen_map_close(json_gen);
    free_pnt_stack(traversal_stack);
    
    return 0;
}

// Thanks to the CompeGPS team for their help!

Position2D sin_projection(const Position2D * const position)
{
    const Position2D position_sp = {
        .latitude = position->latitude * cosf(DEG_TO_RAD(position->longitude)),
        .longitude = position->longitude
    };
    return position_sp;
}

Position2D flat_projection(const Position2D * const position)
{
    const Position2D position_sp = {
        .latitude = position->latitude / cosf(DEG_TO_RAD(position->longitude)),
        .longitude = position->longitude
    };
    return position_sp;
}

Meters geoidal_distance_to_meters(const Dimension d)
{
    return (Meters) d * (Meters) (EARTH_CIRCUMFERENCE / 360.0);
}

Dimension meters_to_geoidal_distance(const Meters d)
{
    return (Dimension) d / (Dimension) (EARTH_CIRCUMFERENCE / 360.0);
}

Dimension2 compute_square_distance(const PanDB * const pan_db,
                                   const Position2D * const position1,
                                   const Position2D * const position2)
{
    Dimension2 d_latitude = (Dimension2) position2->latitude -
        (Dimension2) position1->latitude;
    Dimension2 d_longitude = (Dimension2) position2->longitude -
        (Dimension2) position1->longitude;
    
    if (pan_db == NULL) {
        return (Dimension2) 0.0;
    }
    if (pan_db->layer_type != LAYER_TYPE_FLAT) {
        assert(pan_db->layer_type == LAYER_TYPE_FLATWRAP);
        
        if (d_latitude > pan_db->qbounds.edge1.latitude) {
            d_latitude = d_latitude - pan_db->qbounds.edge1.latitude +
                pan_db->qbounds.edge0.latitude;
        }
        if (d_longitude > pan_db->qbounds.edge1.longitude) {
            d_longitude = d_longitude - pan_db->qbounds.edge1.longitude +
                    pan_db->qbounds.edge0.longitude;
        }
    }
    return d_latitude * d_latitude + d_longitude * d_longitude;
}

Meters distance_between_flat_positions(const PanDB * const pan_db,
                                       const Position2D * const p1,
                                       const Position2D * const p2)
{
    assert(pan_db->layer_type == LAYER_TYPE_FLAT ||
           pan_db->layer_type == LAYER_TYPE_FLATWRAP);
    
    return (Meters) sqrtf(compute_square_distance(pan_db, p1, p2));
}

// Adapted from an implementation by Chris Veness

Meters vincenty_distance_between_geoidal_positions(const Position2D * const p1,
                                                   const Position2D * const p2)
{
    const double a = 6378137.0;
    const double b = 6356752.3142;
    const double f = 1.0 / 298.257223563;
    const double L = DEG_TO_RAD(p2->longitude - p1->longitude);
    const double U1 = atan((1.0 - f) * tan(DEG_TO_RAD(p1->latitude)));
    const double U2 = atan((1.0 - f) * tan(DEG_TO_RAD(p2->latitude)));
    const double sinU1 = sin(U1);
    const double cosU1 = cos(U1);
    const double sinU2 = sin(U2);
    const double cosU2 = cos(U2);    
    double lambda = L;
    double lambdaP;
    double sinSigma;
    double cosSigma;
    double cosSqAlpha;
    double cos2SigmaM;
    double sigma = 0.0;
    unsigned int iterLimit = 100U;
    do {
        const double sinLambda = sin(lambda);
        const double cosLambda = cos(lambda);
        sinSigma = sqrt((cosU2 * sinLambda) * (cosU2 * sinLambda) + 
                        (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda) *
                        (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda));
        if (sinSigma == 0.0) {
            return (Meters) 0.0;
        }
        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
        sigma = atan2(sinSigma, cosSigma);
        const double sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
        cosSqAlpha = 1.0 - sinAlpha * sinAlpha;
        cos2SigmaM = cosSigma - 2.0 * sinU1 * sinU2 / cosSqAlpha;
        if (isnan(cos2SigmaM)) {
            cos2SigmaM = 0.0;
        }
        const double C = f / 16.0 * cosSqAlpha * (4.0 + f * (4.0 - 3.0 * cosSqAlpha));
        lambdaP = lambda;
        lambda = L + (1.0 - C) * f * sinAlpha *
            (sigma + C * sinSigma *
                (cos2SigmaM + C * cosSigma *
                    (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM))
            );
    } while (fabs(lambda-lambdaP) > 1E-12 && --iterLimit > 0U);
    
    if (iterLimit == 0) {
        return hs_distance_between_geoidal_positions(p1, p2);
    }
    const double uSq = cosSqAlpha * (a * a - b * b) / (b * b);
    const double A = 1.0 + uSq / 16384.0 *
        (4096.0 + uSq *
            (-768.0 + uSq * (320.0 - 175.0 * uSq))
        );
    const double B = uSq / 1024.0 *
        (256.0 + uSq *
            (-128.0 + uSq * (74.0 - 47.0 * uSq))
        );
    const double deltaSigma = B * sinSigma *
        (cos2SigmaM + B / 4.0 *
            (cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM) -
                B / 6.0 * cos2SigmaM *
                (-3.0 + 4.0 * sinSigma * sinSigma) *
                (-3.0 + 4.0 * cos2SigmaM * cos2SigmaM)
            )
        );
    const double d = b * A * (sigma - deltaSigma);
    
    return (Meters) d;
}

Meters hs_distance_between_geoidal_positions(const Position2D * const p1,
                                             const Position2D * const p2)
{
    const float lat1 = (float) DEG_TO_RAD(p1->latitude);
    const float lon1 = (float) DEG_TO_RAD(p1->longitude);
    const float lat2 = (float) DEG_TO_RAD(p2->latitude);
    const float lon2 = (float) DEG_TO_RAD(p2->longitude);
    const float dlat = lat2 - lat1;
    const float dlon = lon2 - lon1;
    const float sin_dlath = sinf(dlat / 2.0F);
    const float sin_dlat2 = sin_dlath * sin_dlath;    
    const float sin_dlonh = sinf(dlon / 2.0F);
    const float sin_dlon2 = sin_dlonh * sin_dlonh;
    const float a = sin_dlat2 + cosf(lat1) * cosf(lat2) * sin_dlon2;
    const float c = 2.0F * atan2f(sqrtf(a), sqrtf(1.0F - a));
    const float d = EARTH_RADIUS * c;
    
    return (Meters) d;
}

Meters gc_distance_between_geoidal_positions(const Position2D * const p1,
                                             const Position2D * const p2)
{
    const float a = (float) DEG_TO_RAD(p1->latitude);
    const float b = (float) DEG_TO_RAD(p2->latitude);
    const float x = (float) DEG_TO_RAD(- p1->longitude);
    const float y = (float) DEG_TO_RAD(- p2->longitude);
    const float cosa = sinf(a) * sinf(b) + cosf(a) * cosf(b) * cosf(x - y);
    float d = 0.0F;
    if (cosa > 1.0) {
        return 0.0F;
    }
    if (cosa < (1.0F - FLT_EPSILON)) {
        d = acosf(cosa);
    }
    d *= EARTH_RADIUS;
    if (d < 10.0F) {
        d = fast_distance_between_geoidal_positions(p1, p2);
    }
    return d;
}

Meters fast_distance_between_geoidal_positions(const Position2D * const p1,
                                               const Position2D * const p2)
{
    const float k = cosf(DEG_TO_RAD(p1->latitude));
    const float dx = k * (p1->longitude - p2->longitude);
    const float dy = p1->latitude - p2->latitude;
    const float d = DEG_AVG_DISTANCE * sqrtf(dx * dx + dy * dy);
    
    return (Meters) d;
}

Meters rhomboid_distance_between_geoidal_positions(const Position2D * const p1,
                                                   const Position2D * const p2)
{
    const float k = cosf(DEG_TO_RAD(p1->latitude));
    const float dx = k * (p1->longitude - p2->longitude);
    const float dy = p1->latitude - p2->latitude;
    const float d = DEG_AVG_DISTANCE * (fabs(dx) + fabs(dy));
    
    return (Meters) d;
}

void untangle_rect(Rectangle2D * const rect)
{
    if (rect->edge0.latitude > rect->edge1.latitude) {
        const Dimension tmp = rect->edge0.latitude;
        rect->edge0.latitude = rect->edge1.latitude;
        rect->edge0.latitude = tmp;
    }
    if (rect->edge0.longitude > rect->edge1.longitude) {
        const Dimension tmp = rect->edge0.longitude;
        rect->edge0.longitude = rect->edge1.longitude;
        rect->edge1.longitude = tmp;
    }
}

int safe_write(const int fd, const void * const buf_, size_t count,
               const int timeout)
{
    const char *buf = (const char *) buf_;
    ssize_t written;
    struct pollfd pfd;
    
    pfd.fd = fd;
    pfd.events = POLLOUT;
    
    while (count > (size_t) 0) {
        for (;;) {
            if ((written = write(fd, buf, count)) <= (ssize_t) 0) {
                if (errno == EAGAIN) {
                    if (poll(&pfd, (nfds_t) 1, timeout) == 0) {
                        errno = ETIMEDOUT;
                        return -1;
                    }
                } else if (errno != EINTR) {
                    return -1;
                }
                continue;
            }
            break;
        }
        buf += written;
        count -= written;
    }
    return 0;
}

ssize_t safe_read(const int fd, void * const buf_, size_t count)
{
    unsigned char *buf = (unsigned char *) buf_;
    ssize_t readnb;
    
    do {
        while ((readnb = read(fd, buf, count)) < (ssize_t) 0 &&
               errno == EINTR);
        if (readnb < (ssize_t) 0 || readnb > (ssize_t) count) {
            return readnb;
        }
        if (readnb == (ssize_t) 0) {
ret:
            return (ssize_t) (buf - (unsigned char *) buf_);
        }
        count -= readnb;
        buf += readnb;
    } while (count > (ssize_t) 0);
    goto ret;
}

int fcntl_or_flags(const int socket, const int or_flags)
{
    int flags;
    
    if ((flags = fcntl(socket, F_GETFL, 0)) == -1) {
        flags = 0;
    }
    return fcntl(socket, F_SETFL, flags | or_flags);
}

int fcntl_nand_flags(const int socket, const int nand_flags)
{
    int flags;
    
    if ((flags = fcntl(socket, F_GETFL, 0)) == -1) {
        flags = 0;
    }
    return fcntl(socket, F_SETFL, flags & ~nand_flags);
}

int init_binval(BinVal * const binval)
{
    *binval = (BinVal) {
        .val = NULL,
        .size = (size_t) 0U,
        .max_size = (size_t) 0U
    };
    return 0;
}

void free_binval(BinVal * const binval)
{
    if (binval == NULL) {
        return;
    }
    free(binval->val);
    init_binval(binval);
}

int append_to_binval(BinVal * const binval, const char * const str,
                     const size_t size)
{
    char *tmp_buf;    
    const size_t free_space = binval->max_size - binval->size;
    
    if (free_space < size) {
        const size_t wanted_max_size = binval->size + size;
        if (wanted_max_size < binval->size || wanted_max_size < size) {
            return -1;
        }
        if ((tmp_buf = realloc(binval->val,
                               wanted_max_size + (size_t) 1U)) == NULL) {
            return -1;
        }
        binval->val = tmp_buf;
        binval->max_size = wanted_max_size;
    }
    assert(binval->size + size <= binval->max_size);
    memcpy(binval->val + binval->size, str, size);
    binval->size += size;
    *(binval->val + binval->size) = 0;
    
    return 0;
}

int init_buffered_read(BufferedReadContext * const context,
                       const int fd)
{
    *context = (BufferedReadContext) {
        .buf = NULL,
        .fd = fd,
        .offset = (off_t) 0,
        .total_size = (off_t) 0,
        .buffer_size = DEFAULT_BUFFERED_READ_BUFFER_SIZE
    };
    struct stat st;
    
    if (fstat(fd, &st) != 0) {
        return -1;
    }
    context->total_size = st.st_size;
    struct evbuffer *buf = evbuffer_new();
    if (buf == NULL) {
        return -1;
    }
    context->buf = buf;
    
    return 0;
}

void free_buffered_read(BufferedReadContext * const context)
{
    evbuffer_free(context->buf);
    *context = (BufferedReadContext) {
        .buf = NULL,
        .fd = -1,
        .offset = (off_t) 0,            
        .total_size = (off_t) 0,
        .buffer_size = DEFAULT_BUFFERED_READ_BUFFER_SIZE
    };
}

ssize_t buffered_read(BufferedReadContext * const context,
                      char * const out_buf, const size_t length)
{
    struct evbuffer * const buf = context->buf;
    const size_t available_in_buf = evbuffer_get_length(buf);
    if (available_in_buf < length) {
        if (context->offset >= context->total_size) {
            return 0;
        }        
        size_t to_read = length - available_in_buf;
        const off_t remaining = context->total_size - context->offset;
        if ((off_t) to_read > remaining) {
            return 0;
        }
        if (remaining - (off_t) to_read >= (off_t) context->buffer_size) {
            to_read += context->buffer_size;
        }
        const ssize_t readnb =
            (ssize_t) evbuffer_read(buf, context->fd, to_read);
        if (readnb <= (ssize_t) 0) {
            return readnb;
        }
        context->offset += (off_t) readnb;
        assert(context->offset <= context->total_size);
    }
    if (evbuffer_remove(buf, out_buf, length) <= 0) {
        return -1;
    }
    return (ssize_t) length;
}

static unsigned int open_max(void)
{
    long z;
    
    if ((z = (long) sysconf(_SC_OPEN_MAX)) < 0L) {
        logfile_error(NULL, "_SC_OPEN_MAX");
        return 2U;
    }
    return (unsigned int) z;
}

static int closedesc_all(const int closestdin)
{
    int fodder;
    
    if (closestdin != 0) {
        (void) close(0);
        if ((fodder = open("/dev/null", O_RDONLY)) == -1) {
            return -1;
        }
        (void) dup2(fodder, 0);
        if (fodder > 0) {
            (void) close(fodder);
        }
    }
    if ((fodder = open("/dev/null", O_WRONLY)) == -1) {
        return -1;
    }
    (void) dup2(fodder, 1);
    (void) dup2(1, 2);
    if (fodder > 2) {
        (void) close(fodder);
    }
    
    return 0;
}

int do_daemonize(void)
{
    pid_t child;
    unsigned int i;
    
    if ((child = fork()) == (pid_t) -1) {
        logfile_error(NULL, "Unable to fork() in order to daemonize");
        return -1;
    } else if (child != (pid_t) 0) {
        _exit(0);
    }
    if (setsid() == (pid_t) -1) {
        logfile_error(NULL, "Unable to setsid()");
    }
    i = open_max();
    do {
        if (isatty((int) i)) {
            (void) close((int) i);
        }
        i--;
    } while (i > 2U);
    if (closedesc_all(1) != 0) {
        logfile_error(NULL, "/dev/null duplication");
        return -1;
    }        
    return 0;
}

uint32_t pm_rand(void)
{
    static uint32_t seed = 42U;
    
    uint32_t lo = 16807U * (seed & 0xffff);
    uint32_t hi = 16807U * (seed >> 16);
    lo += (hi & 0x7fff) << 16;
    lo += hi >> 15;
    if (lo > 0x7fffffff) {
        lo -= 0x7fffffff;
    }
    seed = lo;
    
    return seed;
}
