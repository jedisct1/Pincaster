
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__ 1

#ifndef NB_WORKERS
# define NB_WORKERS         10U
#endif
#ifndef MAX_QUEUED_REPLIES
# define MAX_QUEUED_REPLIES 10000U
#endif
#ifndef BUCKET_SIZE
# define BUCKET_SIZE 50U
#endif
#ifndef DEFAULT_DIMENSION_ACCURACY
# define DEFAULT_DIMENSION_ACCURACY ((Dimension) 0.001)
#endif
#ifndef DEFAULT_ACCURACY
# define DEFAULT_ACCURACY ACCURACY_FAST
#endif
#ifndef DEFAULT_LAYER_TYPE
# define DEFAULT_LAYER_TYPE LAYER_TYPE_ELLIPSOIDAL
#endif
#ifndef DEFAULT_SERVER_PORT
# define DEFAULT_SERVER_PORT "4269"
#endif
#ifndef DEFAULT_REPLICATION_PORT
# define DEFAULT_REPLICATION_PORT "4270"
#endif
#ifndef DEFAULT_CLIENT_TIMEOUT
# define DEFAULT_CLIENT_TIMEOUT 60
#endif
#ifndef DEFAULT_JOURNAL_BUFFER_SIZE
# define DEFAULT_JOURNAL_BUFFER_SIZE 4096U
#endif
#ifndef DEFAULT_FSYNC_PERIOD
# define DEFAULT_FSYNC_PERIOD 5
#endif

#define PROJECTION 0

int parse_config(const char * const file);
int check_sys_config(void);
void free_config(void);

#endif
