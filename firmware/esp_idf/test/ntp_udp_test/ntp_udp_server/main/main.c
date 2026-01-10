#include <ntp_udp_server.h>

void app_main() {
    init_server_wifi();
    start_server_timesync_loop();
}