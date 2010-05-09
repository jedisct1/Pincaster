
#ifndef __UTILS_H__
#define __UTILS_H__ 1

#define BINVAL_IS_EQUAL_TO_CONST_STRING(BV, S) \
    ((BV)->size == sizeof (S) - (size_t) 1U && \
        strncasecmp((BV)->val, (S), sizeof (S) - (size_t) 1U) == 0)

#define DEG_TO_RAD(X) ((X) * M_PI / 180.0)

typedef struct BinVal_ {
    char *val;
    size_t size;
    size_t max_size;
} BinVal;

void skip_spaces(const char * * const str);

int key_node_to_json(KeyNode * const key_node, yajl_gen json_gen,
                     const _Bool with_properties);

Position2D sin_projection(const Position2D * const position);
Position2D flat_projection(const Position2D * const position);
Meters geoidal_distance_to_meters(const Dimension d);
Dimension meters_to_geoidal_distance(const Meters d);

Dimension2 compute_square_distance(const PanDB * const pan_db,
                                   const Position2D * const position1,
                                   const Position2D * const position2);

Meters distance_between_flat_positions(const PanDB * const pan_db,
                                       const Position2D * const p1,
                                       const Position2D * const p2);

Meters hs_distance_between_geoidal_positions(const Position2D * const p1,
                                             const Position2D * const p2);

Meters gc_distance_between_geoidal_positions(const Position2D * const p1,
                                             const Position2D * const p2);

Meters fast_distance_between_geoidal_positions(const Position2D * const p1,
                                               const Position2D * const p2);

Meters romboid_distance_between_geoidal_positions(const Position2D * const p1,
                                                  const Position2D * const p2);

int safe_write(const int fd, const void * const buf_, size_t count,
               const int timeout);
ssize_t safe_read(const int fd, void * const buf_, size_t maxlen);

int fcntl_or_flags(const int socket, const int or_flags);
int fcntl_nand_flags(const int socket, const int nand_flags);

int init_binval(BinVal * const binval);
void free_binval(BinVal * const binval);

int append_to_binval(BinVal * const binval, const char * const str,
                     const size_t size);

#endif
