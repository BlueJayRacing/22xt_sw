#ifndef MESSAGE_QUEUE

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

static const char* STAG = "socket handler";

class SocketHandler {
    public:
        SocketHandler();
        ~SocketHandler();

        esp_err_t init(int port, char * ip_addr);

        esp_err_t send(uint8_t buf[], size_t buf_len);
        bool recv(uint8_t * but);

    private:
        SemaphoreHandle_t mutex;

        struct timeval timeout;

        struct sockaddr_in dest_addr;
        struct sockaddr_storage source_addr;
        socklen_t socklen;

};

#endif