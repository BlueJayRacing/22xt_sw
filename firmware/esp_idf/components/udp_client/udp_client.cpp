#include "udp_client.hpp"
#include <lockGuard.hpp>
#include <algorithm>

static const char* TAG = "udp_client";

ESP_EVENT_DEFINE_BASE(SENDER_EVENT_BASE);


uint64_t get_timestamp() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int64_t) time.tv_sec * 1000000L + (int64_t) time.tv_usec;
}

UdpClient::UdpClient() {
    recv_queue = xQueueCreate(30, sizeof(Message *));
}

UdpClient::~UdpClient() {
    Message * msg;
    // memset(msg, 0, sizeof(*msg));
    while(xQueueReceive(recv_queue, &msg, 0)) {
        delete msg;
    }

    esp_event_loop_delete(sender_loop_handle);
}

bool UdpClient::is_wifi_connected() {
    wifi_ap_record_t ap_rec;
    return esp_wifi_sta_get_ap_info(&ap_rec) == ESP_OK;
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

esp_err_t UdpClient::ensure_wifi_connection(int max_attempts) {
    wifi_ap_record_t ap_rec;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_rec);

    for (int i = 0; i < max_attempts; i++) {
        if (err == ESP_OK) {
            return err;
        }

        esp_wifi_connect();
        err = esp_wifi_sta_get_ap_info(&ap_rec);
        vTaskDelay(50);
    }

    ESP_LOGE(TAG, "Cannot connect to wifi");
    return err;
}

esp_err_t UdpClient::initialize_wifi_connection() {
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

    esp_netif_t * wifi_netif_ = esp_netif_create_default_wifi_sta();
    if (wifi_netif_ == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA interface");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&wifi_initiation);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi (err: %d)\n", err);
        return err;
    }

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "baja",
            }};

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi Mode (err: %d)\n", err);
        return err;
    }    

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi Mode (err: %d)\n", err);
        return err;
    }
    
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi (err: %d)\n", err);
        return err;
    }

    err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fAILED TO SCAN");
        return err;
    }

    // wifi_ap_record_t records[15];
    // uint16_t len = 15;
    // err = esp_wifi_scan_get_ap_records(&len, records);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to get ap err %s", esp_err_to_name(err));
    //     return err;
    // }

    // for (int i = 0; i < len; i++) {
    //     ESP_LOGI(TAG, "ap name: %s", records[i].ssid);
    // }


    err = ensure_wifi_connection(5);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to connect to ap");
        return err;
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "wifi initialized");

    return ESP_OK;
}

esp_err_t UdpClient::initialize_socket() {
    esp_err_t err = ensure_wifi_connection(10);
    if (err != ESP_OK) {
        return err;
    }

    esp_event_loop_args_t sender_loop_args = {
        .queue_size = 10,
        .task_name = "sender event",
        .task_priority = 2,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };

    err = esp_event_loop_create(&sender_loop_args, &sender_loop_handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create the sender event loop: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_register_with(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, udp_send_event_handler, (void *) this);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register the sender event loop: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t rtos_err = xTaskCreate(udpListenerWorker, "receiver thread", 1<<12, (void *) this, 2, NULL);
    if (rtos_err != pdPASS) {
        ESP_LOGE(TAG, "Failed to create listener worker task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t UdpClient::publish_data(uint64_t timestamp_, std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buff_size) {    
    if (buff_size > MESSAGE_MAX_LEN) {
        ESP_LOGE(TAG, "Published message size is too long");
        return ESP_FAIL;
    }
    
    Message * msg = new Message();
    memset(msg, 0, sizeof(Message));
    msg->timestamp = timestamp_;
    msg->payload = buf;
    msg->payload_len = buff_size;

    // udp_send_event_handler((void *) this, NULL, 0, (void*)msg);
    esp_err_t err = esp_event_post_to(sender_loop_handle, SENDER_EVENT_BASE, SENDER_EVENT_ID, msg, sizeof(Message), 5);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post to sender event loop, err: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

void UdpClient::udp_send_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
    UdpClient * client = (UdpClient *) handler_arg;
    Message * msg = (Message *) event_data;

    if (msg == nullptr) return;

    if (!client->is_wifi_connected()) {
        client->ensure_wifi_connection(5);
        ESP_LOGE(TAG, "Disconnected from ap");
    }

    ESP_LOGI(TAG, "Sending message");
    client->socket_handler_.send(msg->payload, msg->payload_len);
    // delete msg;
}

Message * UdpClient::recv_data() {
    Message * msg;
    if (xQueueReceive(recv_queue, &msg, 10) == pdPASS) {
        return msg;
    }

    return nullptr;
}

void UdpClient::udpListenerWorker(void * pvParamter) {

    UdpClient * client = (UdpClient *) pvParamter;

    while (1) {
        esp_err_t err = client->socket_handler_.init(PORT, HOST_IP_ADDR);
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to initialize socket: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        while (1) {
            vTaskDelay(10);

            if(!client->is_wifi_connected()) {
                client->ensure_wifi_connection(5);
                ESP_LOGW(TAG, "Disconnected from AP");
            }

            Message * msg = new Message();
            memset(msg, 0, sizeof(Message));
            int len = client->socket_handler_.recv(msg);
            if(len < 0) {
                delete msg;
                continue;
            }

            ESP_LOGI(TAG, "Adding received message to queue");
            msg->timestamp = get_timestamp();
            msg->payload_len = len;
            
            if(xQueueSend(client->recv_queue, (void *) &msg, 0) != pdPASS) {
                ESP_LOGW(TAG, "Failed to add received message to queue");
            }
        }

        client->socket_handler_.close_sock();
    }

    vTaskDelete(NULL);
}
