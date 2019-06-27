#include <stddef.h>

#include "scheduler.h"
#include "logger.h"

static int ipvs_status_dumper(thread_t*);
static thread_t* dumper_thread;

int
status_socket_init(void)
{
    dumper_thread = thread_add_timer(master, ipvs_status_dumper, NULL, 5 * TIMER_HZ);
    log_message(LOG_INFO, "registered dumper_thread");
    return 0;
}

int
status_socket_close(void)
{
    // TODO: close socket
    log_message(LOG_INFO, "terminating dumper_thread");
    thread_cancel(dumper_thread);
    return 0;
}

int
status_socket_reload(void)
{
    if (1 /* TODO: socket config changed */) {
        log_message(LOG_INFO, "reloading dumper_thread");
        status_socket_close();
        status_socket_init();
    }
    return 0;
}

static int
ipvs_status_dumper(thread_t * thread)
{
    log_message(LOG_INFO, "dumper_thread timer event");
    dumper_thread = thread_add_timer(master, ipvs_status_dumper, NULL, 5 * TIMER_HZ);
    return 0;
}
