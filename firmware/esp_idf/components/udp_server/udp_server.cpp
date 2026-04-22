#include "udp_server.hpp"
#include <lockGuard.hpp>
#include <algorithm>

static const char* TAG = "udp_server";

ESP_EVENT_DEFINE_BASE(SENDER_EVENT_BASE);


uint64_t get_timestamp() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int64_t) time.tv_sec * 1000000L + (int64_t) time.tv_usec;
}

UdpServer::UdpServer() {
    recv_queue = xQueueCreate(10, sizeof(Message *));

    // ESP_EVENT_DECLARE_BASE(SENDER_EVENT_BASE);
}

UdpServer::~UdpServer() {
    // Message msg;
    // while(xQueueReceive(recv_queue, pmsg, 0)) {
    //     Message * pmsg = &msg
    //     delete pmsg;
    // }

    esp_event_loop_delete(sender_loop_handle);
}

void UdpServer::clear_recv_queue(void) {
    Message * msg = nullptr;
    while(true) {
        msg = recv_data();
        if (msg == nullptr) delete msg;
    }
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

esp_err_t UdpServer::initialize_wifi_connection() {
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS Flash (err: %d)", err);
        return err;
    }
    err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Network Interface (err: %d)", err);
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Event Loop (err: %d)", err);
        return err;
    }

    esp_netif_t * wifi_netif_ = esp_netif_create_default_wifi_ap();
    if (wifi_netif_ == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi AP interface");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_initiation);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi (err: %d)\n", err);
        return err;
    }

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi Mode (err: %d)\n", err);
        return err;
    }    

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi (err: %d)\n", err);
        return err;
    }

        
    wifi_config_t wifi_configuration = {
        .ap = {
            .ssid = "baja",
            .max_connection = 5
        }};
    
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_configuration);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config (err: %d)\n", err);
        return err;
    }
    
    esp_wifi_connect();

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "wifi initialized");

    return ESP_OK;
}

esp_err_t UdpServer::initialize_socket() {
    esp_err_t err;
    BaseType_t ferr;

    esp_event_loop_args_t sender_loop_args = {
        .queue_size = 10,
        .task_name = "sender event",
        .task_priority = 2,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };

    err = esp_event_loop_create(&sender_loop_args, &sender_loop_handle);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create the event loop: %s", esp_err_to_name(err));
    }

    err = esp_event_handler_register_with(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, udp_send_event_handler, (void *) this);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register the event handler to the event loop: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Starting the listener thread");
    ferr = xTaskCreate(udpListenerWorker, "receiver thread", 2<<14, (void *) this, 4, NULL);
    if(ferr != pdPASS)
    {
        ESP_LOGE(TAG, "Could not allocate required memory");
    }

    return ESP_OK;
}

esp_err_t UdpServer::publish_data(uint64_t timestamp_, std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buff_size, sockaddr_in dest_addr) {    
    if (buff_size > MESSAGE_MAX_LEN) {
        ESP_LOGE(TAG, "Published message size is too long");
        return ESP_FAIL;
    }

    esp_err_t err;
    Message * msg = new Message();
    memset(msg, 0, sizeof(Message));
    msg->timestamp = timestamp_;
    msg->payload = buf;
    msg->payload_len = buff_size;

    ESP_LOGI(TAG, "ESP INFO BUF SIZE %d, %d", buf.size(), msg->payload_len);
    msg->addr = dest_addr;
    
    err = esp_event_post_to(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, msg, sizeof(Message), 5);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register the event handler to the event loop: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

void UdpServer::udp_send_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
    ESP_LOGI(TAG, "called send event handler");
    char addr_str[128];
    UdpServer * server = (UdpServer *) handler_arg;
    Message * msg = (Message *) event_data;

    if (msg == nullptr) return;

    inet_ntoa_r(msg->addr.sin_addr, addr_str, sizeof(addr_str) - 1);
    ESP_LOGI(TAG, "message payload len %s, %d", addr_str, msg->payload_len);


    for (int i = 0; i < msg->payload_len; i++) {
        ESP_LOGE(TAG, "DATA SEND %d: %d", i, msg->payload[i]);
    }

    server->socket_handler_.send(msg->payload, msg->payload_len, msg->addr);

    // delete msg;
}

Message * UdpServer::recv_data() {
    Message * msg = nullptr;
    if (xQueueReceive(recv_queue, &msg, 10) == pdPASS) {
        // ESP_LOGI(TAG, "returned message");
        return msg;
    }

    return nullptr;
}

void UdpServer::udpListenerWorker(void * pvParamter) {

    UdpServer * server = (UdpServer *) pvParamter;

    while (1) {
        esp_err_t err = server->socket_handler_.init(PORT);
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to initialize socket");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        BaseType_t ferr;

        while (1) {
            // ESP_LOGI(TAG, "Starting to recv message");
            Message * msg = new Message();
            memset(msg, 0, sizeof(Message));
            int len = server->socket_handler_.srecv(msg);
            if(len < 0) {
                delete msg;
                continue;
            }

            // ESP_LOGI(TAG, "adding message to q");
            // memcpy(msg->buf, buf, sizeof(buf));
            msg->timestamp = get_timestamp();
            msg->payload_len = len;
            
            ferr = xQueueSend(server->recv_queue, (void *) &msg, 0);

            if(ferr != pdPASS)
            {
                ESP_LOGW(TAG, "Queue is full!");
                delete msg;
            }

            // std::fill_n(buf, 20, 0);
        }

        server->socket_handler_.close_sock();

    }
    vTaskDelete(NULL);

}
