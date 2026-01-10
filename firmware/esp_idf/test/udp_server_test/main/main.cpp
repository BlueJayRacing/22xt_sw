#include "udp_server.hpp"
#include <array>

static const char * TAG = "main";

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf) {
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num += buf.at(8) << (i * 8);
    }

    return num;
}

extern "C" void app_main() {
    UdpServer server;
    server.initialize_wifi_connection();
    server.initialize_socket();

    // while(!server.is_socket_init()) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

    while (1) {
        Message * msg = server.recv_data();
        if (msg == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        ESP_LOGI(TAG, "printing message %d", msg->payload_len);
        for(int j = 0; j < msg->payload_len; j++) {
            ESP_LOGI(TAG, "Data %d: %d", j, msg->payload[j]);
        }

        server.publish_data(0, msg->payload, msg->payload_len, msg->addr);
        ESP_LOGI(TAG, "Sent back");

        delete msg;
    }
}