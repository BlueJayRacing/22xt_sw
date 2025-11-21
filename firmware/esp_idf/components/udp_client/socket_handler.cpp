#include "socket_handler.hpp"

SocketHandler::SocketHandler() {}

esp_err_t SocketHandler::init(int port, char * host_ip_addr) {
    socklen = sizeof(source_addr);

    dest_addr.sin_addr.s_addr = inet_addr(host_ip_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    timeout.tv_sec = 2;
    timeout.us_sec = 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(STAG, "Unable to create socket: errno %d", sock);
        return ESP_FAIL;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ESP_LOGI(STAG, "Socket created");

    mutex = xSemaphoreCreateMutex();

    return ESP_OK;
}

esp_err_t send(uint8_t buf[], size_t buf_len) {
    if (buf_len > 20) {
        ESP_LOGE(STAG, "Invalid send buffer size of %d", buf_len);
        return ESP_FAIL;
    }

    lockGuard guard(mutex);
    int err = sendto(sock, buf, buf_len, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));

    if(err < 0) {
        ESP_LOGE(STAG, "Error sending message over socket");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t recv(uint8_t * buf) {
    lockGuard guard(mutex);

    int len = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    if (len < 0) {
        ESP_LOGI(STAG, "Received nothing from socket");
        return false;
    }

    return true;
}