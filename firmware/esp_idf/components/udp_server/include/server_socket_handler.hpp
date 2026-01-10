#ifndef SERVER_SOCKET_HANDLER

#include "esp_log.h"
#include "lockGuard.hpp"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include <lockGuard.hpp>
#include <array>

typedef struct Message {
    uint64_t timestamp;
    std::array<uint8_t, 30> payload;
    size_t payload_len;
    struct sockaddr_in addr;
} Message;


class SocketHandler {
    public:
        SocketHandler();
        ~SocketHandler();

        esp_err_t init(int port);

        esp_err_t send(std::array<uint8_t, 30> buf, size_t buf_len, struct sockaddr_in dest_addr);
        int recv(Message * msg);
        void close_sock();

    private:
        SemaphoreHandle_t mutex;
        int sock;

        struct timeval timeout;

        struct sockaddr_in6 dest_addr6;
        socklen_t socklen;
};

#endif