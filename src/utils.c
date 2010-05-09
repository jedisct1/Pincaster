
#include "common.h"
#include "utils.h"

void skip_spaces(const char * * const str)
{
    const char *s = *str;
    while (*s != 0 && isspace((unsigned char ) *s)) {
        s++;
    }
    *str = s;
}

typedef struct RecordsGetPropertiesCBContext_ {
    yajl_gen json_gen;    
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
    return 0;
}

int key_node_to_json(KeyNode * const key_node, yajl_gen json_gen,
                     const _Bool with_properties)
{
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
    RecordsGetPropertiesCBContext cb_context = {
        .json_gen = json_gen
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
    
    if (pan_db == NULL || pan_db->layer_type != LAYER_TYPE_FLAT) {
        if (pan_db->layer_type == LAYER_TYPE_FLATWRAP) {
            if (d_latitude > pan_db->qbounds.edge1.latitude) {
                d_latitude = d_latitude - pan_db->qbounds.edge1.latitude +
                    pan_db->qbounds.edge0.latitude;
            }
            if (d_longitude > pan_db->qbounds.edge1.longitude) {
                d_longitude = d_longitude - pan_db->qbounds.edge1.longitude +
                    pan_db->qbounds.edge0.longitude;
            }
        } else {
            if (d_latitude > (Dimension) 180.0) {
                d_latitude = (Dimension) 360.0 - d_latitude;
            }
            if (d_longitude > (Dimension) 180.0) {
                d_longitude = (Dimension) 360.0 - d_longitude;
            }
        }
    }
    return d_latitude * d_latitude + d_longitude * d_longitude;
}

Meters distance_between_flat_positions(const PanDB * const pan_db,
                                       const Position2D * const p1,
                                       const Position2D * const p2)
{
    return (Meters) sqrtf(compute_square_distance(pan_db, p1, p2));
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
    const float sin_dlath = sinf(dlat / 2.0);
    const float sin_dlat2 = sin_dlath * sin_dlath;    
    const float sin_dlonh = sinf(dlon / 2.0);
    const float sin_dlon2 = sin_dlonh * sin_dlonh;
    const float a = sin_dlat2 + cosf(lat1) * cosf(lat2) * sin_dlon2;
    const float c = 2.0 * atan2f(sqrtf(a), sqrtf(1.0 - a));
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
    if (cosa >= 1.0F) {
        return 0.0F;
    }
    if (cosa < 0.999999999999999F) {
        d = acos(cosa);
    }
    d *= EARTH_RADIUS;
    if (d < 10.0F) {
        d = distance_between_flat_positions(NULL, p1, p2);
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

Meters romboid_distance_between_geoidal_positions(const Position2D * const p1,
                                                  const Position2D * const p2)
{
    const float k = cosf(DEG_TO_RAD(p1->latitude));
    const float dx = k * (p1->longitude - p2->longitude);
    const float dy = p1->latitude - p2->latitude;
    const float d = DEG_AVG_DISTANCE * (fabs(dx) + fabs(dy));
    
    return (Meters) d;
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

ssize_t safe_read(const int fd, void * const buf_, size_t maxlen)
{
    unsigned char *buf = (unsigned char *) buf_;
    ssize_t readnb;
    
    do {
        while ((readnb = read(fd, buf, maxlen)) < (ssize_t) 0 &&
               errno == EINTR);
        if (readnb < (ssize_t) 0 || readnb > (ssize_t) maxlen) {
            return readnb;
        }
        if (readnb == (ssize_t) 0) {
ret:
            return (ssize_t) (buf - (unsigned char *) buf_);
        }
        maxlen -= readnb;
        buf += readnb;
    } while (maxlen > (ssize_t) 0);
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
