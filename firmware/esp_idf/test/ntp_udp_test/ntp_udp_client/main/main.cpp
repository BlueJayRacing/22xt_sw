#include <ntp_udp_client.hpp>

extern "C" void app_main() {
    init_client_wifi();
    start_client_timesync_loop();
}