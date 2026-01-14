#ifndef CLIENT_SOCKET_HANDLER

#include "esp_log.h"
#include "lockGuard.hpp"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include <lockGuard.hpp>
#include <array>

#define MESSAGE_MAX_LEN 130

typedef struct Message {
    uint64_t timestamp;
    std::array<uint8_t, MESSAGE_MAX_LEN> payload;
    size_t payload_len;
} Message;


class SocketHandler {
    public:
        SocketHandler();
        ~SocketHandler();

        esp_err_t init(int port, char * ip_addr);

        esp_err_t send(std::array<uint8_t, MESSAGE_MAX_LEN> buf, size_t buf_len);
        int recv(Message * msg);

        bool is_socket_open();

        void close_sock();

    private:
        esp_err_t partial_socket_init();

        SemaphoreHandle_t mutex;
        int sock;

        int port;

        struct timeval timeout;

        struct sockaddr_in dest_addr;
        socklen_t socklen;
};

#endif