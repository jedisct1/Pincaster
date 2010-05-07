
#define DEFINE_GLOBALS 1

#include "common.h"
#include "http_server.h"
#include "app_config.h"

static void usage(void)
{
    puts("\nUsage: pincaster <configuration file>\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }
    check_sys_config();
    init_db_log();
    if (parse_config(argv[1]) != 0) {
        return 2;
    }
    open_db_log();
    http_server();
    free_db_log();
    free_config();
    
    return 0;
}
