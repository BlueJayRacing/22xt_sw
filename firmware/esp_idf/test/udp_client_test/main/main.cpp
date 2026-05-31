#include "udp_client.hpp"
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
    UdpClient client;
    client.initialize_wifi_connection();
    client.initialize_socket();

    std::array<uint8_t, MESSAGE_MAX_LEN> buf;

    for(int i = 0; i < 100; i++) {
        for(int k = 0; k < buf.size(); k++) {
            buf[k] = i;
        }

        client.publish_data(0, buf, buf.size());

        vTaskDelay(pdMS_TO_TICKS(2000));
        Message * msg = client.recv_data();
        if (msg != nullptr) {
            for(int j = 0; j < msg->payload_len; j++) {
                ESP_LOGI(TAG, "Data %d: %d", j, msg->payload[j]);
            }

            delete msg;
        }
    }
}

// 1011 0001 1111
// [1011 , 0001 , 1111 ]