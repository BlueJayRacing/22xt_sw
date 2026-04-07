#include "client_socket_handler.hpp"

static const char* TAG = "socket handler";

SocketHandler::SocketHandler() {
    mutex = xSemaphoreCreateMutex();
}

SocketHandler::~SocketHandler() {
    shutdown(sock, 0);
}

esp_err_t SocketHandler::init(int port_, char * host_ip_addr) {
    socklen = sizeof(sockaddr_in);

    ESP_LOGW(TAG, "IP ADDR HOST: %s, PORT: %d", host_ip_addr, port_);

    dest_addr.sin_addr.s_addr = inet_addr(host_ip_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_);

    return partial_socket_init();
}

esp_err_t SocketHandler::partial_socket_init() {
    timeout.tv_sec = 10;
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

bool SocketHandler::is_socket_open() {
    int error_code;
    socklen_t error_code_size = sizeof(error_code);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);

    return error_code >= 0;
}

esp_err_t SocketHandler::send(std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buf_len) {
    if (buf_len > 130) {
        ESP_LOGE(TAG, "Invalid send buffer size of %d", buf_len);
        return ESP_FAIL;
    }

    lockGuard guard(mutex);
    if (!is_socket_open()) {
        partial_socket_init();
    }

    ESP_LOGI(TAG, "sending %d %d", is_socket_open(), buf_len);
    // for (int i = 0; i < buf.size(); i ++) {
    //     ESP_LOGI(TAG, "%d: %d", i, buf[i]);
    // }

    int err = sendto(sock, buf.data(), buf_len, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));

    if(err < 0) {
        ESP_LOGE(TAG, "Error sending message over socket");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Sent %d bytes", err);

    return ESP_OK;
}

int SocketHandler::recv(Message * msg) {
    lockGuard guard(mutex);
    sockaddr_in source_addr;
    socklen_t size = sizeof(sockaddr_in);

    // int server_sock = accept(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
    // if (server_sock < 0) {
    //     ESP_LOGI(TAG, "socket doesn't have any data");
    //     return -1;
    // }

    if (!is_socket_open()) {
        partial_socket_init();
    }

    int len = recvfrom(sock, msg->payload.data(), msg->payload.size(), 0, (struct sockaddr *)&source_addr, &size);
    if (len < 0) {
        ESP_LOGI(TAG, "Received nothing from socket");
        return -1;
    }

    ESP_LOGI(TAG, "Message received from socket");
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