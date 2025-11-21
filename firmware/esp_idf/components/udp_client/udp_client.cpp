#include "udp_client.hpp"
#include <lockGuard.hpp>
#include <algorithm>

uint64_t get_timestamp() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int64_t) time.tv_sec * 1000000L + (int64_t) time.tv_usec;
}

UdpClient::UdpClient() {}

UdpClient::~UdpClient() {
    // Message * msg;
    // while(xQueueReceive(recv_queue, msg, 0)) {
    //     delete &msg;
    // }

    esp_event_loop_delete(sender_loop_handle);
}

bool UdpClient::is_wifi_connected() {
    wifi_ap_record_t ap_rec;
    return esp_wifi_sta_get_ap_info(&ap_rec) == ESP_OK;
}

esp_err_t UdpClient::ensure_wifi_connection(int max_attempts) {
    esp_err_t err = ESP_OK;
    wifi_ap_record_t ap_rec;
    for (int i = 0; i < max_attempts; i++) {
        esp_wifi_connect();
        err = esp_wifi_sta_get_ap_info(&ap_rec);
        if (err == ESP_OK) {
            return err;
        }
        vTaskDelay(50);
    }

    ESP_LOGE(TAG, "Cannot connect to wifi");
    return err;
}

esp_err_t UdpClient::initialize_wifi() {

    esp_err_t err = socket_handler_.init(PORT, HOST_IP_ADDR);
    if (err != ESP_OK) {
        return err;
    }

    err = ensure_wifi_connection(10);
    if (err != ESP_OK) {
        return err;
    }

    // send_queue = xQueueCreate(10, sizeof(Message *));
    recv_queue = xQueueCreate(10, sizeof(Message *));

    esp_event_loop_args_t sender_loop_args = {
        .queue_size = 10,
        .task_name = "sender event",
        .task_priority = 2,
        .task_stack_size = 2048,
        .task_core_id = 2
    };

    // ESP_EVENT_DECLARE_BASE(SENDER_EVENT_BASE);
    // ESP_EVENT_DEFINE_BASE(SENDER_EVENT_BASE);

    esp_event_loop_create(&sender_loop_args, &sender_loop_handle);
    esp_event_handler_register_with(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, udp_send_event_handler, nullptr);

    xTaskCreate(udpListenerWorker, "receiver thread", 2048, nullptr, (UBaseType_t) 1, recv_task_handle);

    return ESP_OK;
}

esp_err_t UdpClient::publish_data(uint64_t timestamp_, uint8_t buf[], size_t buff_size) {    
    size_t true_size = std::max(buff_size, (size_t) 20);
    
    Message * msg = new Message();
    msg->timestamp = timestamp_;
    memcpy(msg->buf, buf, true_size * sizeof(uint8_t));
    msg->msg_len = true_size;

    esp_event_post_to(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, msg, sizeof(Message *), 5);

    return ESP_OK;
}

// void UdpClient::udpSenderWorker() {
//     lockGuard guard(mut);

//     Message * msg = send_queue->dequeue();
//     int err = sendto(sock, msg->buf, msg->msg_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

//     delete msg;

//     if (err < 0) {
//         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
//     }
// }

void UdpClient::udp_send_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
    Message * msg = static_cast<Message *>(event_data);

    socket_handler_.send(msg->buf, msg->msg_len);

    delete msg;
}

Message * UdpClient::recv_data() {
    Message * msg;
    xQueueReceive(recv_queue, &msg, 10);

    return msg;
}

void UdpClient::udpListenerWorker(void *) {
    uint8_t buf[20];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if(!socket_handler_.recv(buf)) {
            continue;
        }

        Message * msg = new Message();
        memcpy(msg->buf, buf, sizeof(buf));
        msg->timestamp = get_timestamp();
        msg->msg_len = 20;
        
        xQueueSend(recv_queue, msg, 0);

        std::fill_n(buf, 20, 0);

    }
}


