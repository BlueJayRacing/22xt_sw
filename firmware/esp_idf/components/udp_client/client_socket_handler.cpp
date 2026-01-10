#include "client_socket_handler.hpp"

static const char* TAG = "socket handler";

SocketHandler::SocketHandler() {
    mutex = xSemaphoreCreateMutex();
}

SocketHandler::~SocketHandler() {
    shutdown(sock, 0);
}

esp_err_t SocketHandler::init(int port, char * host_ip_addr) {
    socklen = sizeof(sockaddr_in);

    dest_addr.sin_addr.s_addr = inet_addr(host_ip_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", sock);
        return ESP_FAIL;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ESP_LOGI(TAG, "Socket created");

    return ESP_OK;
}

esp_err_t SocketHandler::send(std::array<uint8_t, 30> buf, size_t buf_len) {
    if (buf_len > 30) {
        ESP_LOGE(TAG, "Invalid send buffer size of %d", buf_len);
        return ESP_FAIL;
    }

    // lockGuard guard(mutex);
    int err = sendto(sock, buf.data(), buf_len, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));

    if(err < 0) {
        ESP_LOGE(TAG, "Error sending message over socket");
        return ESP_FAIL;
    }

    return ESP_OK;
}

int SocketHandler::recv(Message * msg) {
    // lockGuard guard(mutex);
    sockaddr_in source_addr;
    socklen_t size = sizeof(sockaddr_in);

    uint8_t buf[30];
    // void * test = msg->payload.data();
    // size_t s = msg->payload.size();
    ESP_LOGI(TAG, "starting recvfrom");
    // int len = 1;
    int len = recvfrom(sock, msg->payload.data(), msg->payload.size(), 0, (struct sockaddr *)&source_addr, &size);
    if (len < 0) {
        ESP_LOGI(TAG, "Received nothing from socket");
        return -1;
    }

    // msg->payload = buf;
    ESP_LOGI(TAG, "message recvfrom");
    msg->payload_len = len;

    return len;
}

void SocketHandler::close_sock() {
    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}