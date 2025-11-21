// UDP Client with WiFi connection communication via Socket

#include <stdio.h>
#include <string.h>
#include <ntp_udp_client.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
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

#define PORT 3333
#define HOST_IP_ADDR "192.168.4.1"
#define US_PER_SECOND 1000000
static const char *TAG = "UDP SOCKET CLIENT";

void sub_timeval(struct timeval t1, struct timeval t2, struct timeval *td)
{
    td->tv_usec = t2.tv_usec - t1.tv_usec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_usec < 0)
    {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_usec > 0)
    {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    }
}

void add_timeval(struct timeval t1, struct timeval t2, struct timeval *td)
{
    td->tv_usec = t2.tv_usec + t1.tv_usec;
    td->tv_sec  = t2.tv_sec + t1.tv_sec;
    if (td->tv_usec >= US_PER_SECOND)
    {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    }
    else if (td->tv_usec <= -US_PER_SECOND)
    {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    }
}

void half_timeval(struct timeval * t1) {
    t1->tv_sec /= 2;
    t1->tv_usec /=2;
}

void ensure_wifi_connection() {
    esp_err_t err;
    wifi_ap_record_t ap_info;
    do {
        esp_wifi_connect();
        err = esp_wifi_sta_get_ap_info(&ap_info);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    } while (err != ESP_OK);
}

static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);


        struct timeval sent_time;

        struct timeval recv_time;

        struct timeval serv_sent_time;
        struct timeval serv_recv_time;
        int64_t serv_sent_time_i;
        int64_t serv_recv_time_i;

        struct timeval cur_time;

        char * first_msg = "first ping from client";

        while (1) {
            // check wifi is connected
            wifi_ap_record_t ap_rec;
            if (esp_wifi_sta_get_ap_info(&ap_rec) != ESP_OK) {
                ensure_wifi_connection();
            }

            // get local time T0
            gettimeofday(&sent_time, NULL);

            // send first msg to server
            int err = sendto(sock, first_msg, strlen(first_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);

            // receive when the server sent this message, and store local timestamp t3
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            gettimeofday(&recv_time, NULL);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                
                // store server sent time t2
                serv_sent_time_i = strtoll(rx_buffer, NULL, 10);
                serv_sent_time.tv_usec = serv_sent_time_i % 10000000L;
                serv_sent_time.tv_sec = (int64_t) (serv_sent_time_i / 1000000L);
            }

            len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                
                // store server recv time t1
                serv_recv_time_i = strtoll(rx_buffer, NULL, 10);
                serv_recv_time.tv_usec = serv_recv_time_i % 1000000L;
                serv_recv_time.tv_sec = (int64_t) serv_recv_time_i / 1000000L;
            }

            struct timeval sub1;
            struct timeval sub2;
            struct timeval offset;
            // = ((serv_recv_time - sent_time) + (serv_sent_time - recv_time)) / 2;
            sub_timeval(serv_recv_time, sent_time, &sub1);
            sub_timeval(serv_sent_time, recv_time, &sub2);
            add_timeval(sub1, sub2, &offset);
            half_timeval(&offset);
           
            gettimeofday(&cur_time, NULL);
            sub_timeval(offset, cur_time, &cur_time);
            ESP_LOGI(TAG, "Err: %d", settimeofday(&cur_time, NULL));
            ESP_LOGI(TAG, "Seconds: %lld, Microseconds: %lld", (int64_t) cur_time.tv_sec, (int64_t) cur_time.tv_usec);

            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
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

void wifi_connection()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "bajatest",
            }};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ensure_wifi_connection();
}

void init_client_wifi(void)
{
    wifi_connection();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}

// will there be problems with no pin to core
void start_client_timesync_loop() {    
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}
