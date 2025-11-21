#include "udp_client.hpp"

extern "C" void app_main() {
    UdpClient client;

    client.initialize_wifi();
}