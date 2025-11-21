#ifndef udp_client

#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_system.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include "nvs_flash.h"
#include "ping/ping_sock.h"
#include "driver/gpio.h"

#include "socket_handler.hpp"

#define PORT 3333
#define HOST_IP_ADDR "192.168.4.1"

// static const int PORT = 3333;
// static const char* HOST_IP_ADDR = "192.168.4.1";
static const char* TAG = "udp_client";

void sub_timeval(struct timeval t1, struct timeval t2, struct timeval *td);
void add_timeval(struct timeval t1, struct timeval t2, struct timeval *td);
void half_timeval(struct timeval * t1);

uint64_t get_timestamp();

enum event_ids : uint32_t {
    SENDER_EVENT_ID = 1,
    RECEIVER_EVENT_ID
};

typedef struct Message {
    uint64_t timestamp;
    uint8_t buf[20];
    size_t msg_len;
} Message;

class UdpClient {
    public:
        UdpClient();
        ~UdpClient();

        static bool is_wifi_connected();
        static esp_err_t initialize_wifi();
        static esp_err_t publish_data(uint64_t timestamp, uint8_t buf[], size_t buff_size);
        static Message * recv_data();
        static void udpListenerWorker(void *);
        // void udpSenderWorker();
        static void udp_send_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
        static void udp_recv_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);


    private:
        static esp_err_t ensure_wifi_connection(int max_attempts);
        
        static QueueHandle_t recv_queue;

        static esp_event_base_t SENDER_EVENT_BASE;
        static esp_event_loop_handle_t sender_loop_handle;

        static TaskHandle_t * recv_task_handle;

        static SocketHandler socket_handler_;
};

#endif