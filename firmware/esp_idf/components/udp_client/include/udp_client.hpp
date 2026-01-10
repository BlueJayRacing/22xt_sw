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

#include "client_socket_handler.hpp"

// #define PORT 3333
// #define HOST_IP_ADDR "192.168.4.1"

static const int PORT = 3333;
static char* HOST_IP_ADDR = "192.168.4.1";

void sub_timeval(struct timeval t1, struct timeval t2, struct timeval *td);
void add_timeval(struct timeval t1, struct timeval t2, struct timeval *td);
void half_timeval(struct timeval * t1);

uint64_t get_timestamp();

enum event_ids : uint32_t {
    SENDER_EVENT_ID = 1,
    RECEIVER_EVENT_ID
};

class UdpClient {
    public:
        UdpClient();
        ~UdpClient();

        bool is_wifi_connected();
        esp_err_t initialize_wifi_connection();
        esp_err_t initialize_socket();
        esp_err_t publish_data(uint64_t timestamp, std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buff_size);
        Message * recv_data();
        static void udpListenerWorker(void *);
        static void send_event_loop_task(void *);
        // void udpSenderWorker();
        static void udp_send_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
        void udp_recv_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);


    private:
        esp_err_t ensure_wifi_connection(int max_attempts);
        QueueHandle_t recv_queue;

        esp_event_loop_handle_t sender_loop_handle;

        TaskHandle_t * recv_task_handle;
        
        SocketHandler socket_handler_;
};

#endif