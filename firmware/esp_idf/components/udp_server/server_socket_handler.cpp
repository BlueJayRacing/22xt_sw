#include "server_socket_handler.hpp"

static const char* TAG = "server socket handler";

SocketHandler::SocketHandler() {
    mutex = xSemaphoreCreateMutex();

}

SocketHandler::~SocketHandler() {
    shutdown(sock, 0);
}

esp_err_t SocketHandler::init(int port_) {
    socklen = sizeof(sockaddr);
    port = port_;
    
    struct sockaddr_in * dest_addr = (struct sockaddr_in *)&dest_addr6;
    dest_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr->sin_family = AF_INET;
    dest_addr->sin_port = htons(port);

    return partial_socket_init();
}

esp_err_t SocketHandler::partial_socket_init() {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", sock);
        return ESP_FAIL;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int err = bind(sock, (struct sockaddr *)&dest_addr6, sizeof(dest_addr6));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Socket created");
    return ESP_OK;
}

bool SocketHandler::is_socket_open() {
    int error_code;
    socklen_t error_code_size = sizeof(error_code);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);

    return error_code >= 0;
}

esp_err_t SocketHandler::send(std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buf_len, struct sockaddr_in dest_addr) {
     lockGuard guard(mutex);
    if (buf_len > MESSAGE_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid send buffer size of %d", buf_len);
        return ESP_FAIL;
    }

    socklen_t socklen = sizeof(dest_addr);

    if (!is_socket_open()) {
        partial_socket_init();
    }

    int err = sendto(sock, buf.data(), buf_len, 0, (struct sockaddr *) &dest_addr, socklen);

    if(err < 0) {
        ESP_LOGE(TAG, "Error sending message over socket %d", err);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sent message over socket %d", err);

    return ESP_OK;
}

int SocketHandler::recv(Message * msg) {
    // lockGuard guard(mutex);

    struct sockaddr_in source_addr;
    socklen_t size = sizeof(sockaddr_in);

    if (!is_socket_open()) {
        partial_socket_init();
    }

    int len = recvfrom(sock, msg->payload.data(), msg->payload.size(), 0, (struct sockaddr *)&source_addr, &size);
    if (len < 0) {
        ESP_LOGI(TAG, "Received nothing from socket");
        return -1;            
    }

    msg->payload_len = len;
    msg->addr = source_addr;

    return len;
}

void SocketHandler::close_sock() {
    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}