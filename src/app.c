
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
    if (app_context.daemonize != 0 && do_daemonize() != 0) {
        return 3;
    }
    if (open_db_log() < 0) {
        return 4;
    }
    if (http_server() != 0) {
        return 5;
    }
    free_db_log();
    free_config();
    
    return 0;
}
