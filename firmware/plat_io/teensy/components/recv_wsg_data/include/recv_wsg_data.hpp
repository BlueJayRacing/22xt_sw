#ifndef RECV_WSG_DATA_H

#include <Arduino.h>
#include <SPI.h>

#include <array>

#define MAX_MESSAGE_LEN 130
#define MESSAGES_PER_DATA_SEND 6
#define SERIALIZED_MSG_SIZE 19

uint64_t buf_to_uint64(uint8_t * start);
uint32_t buf_to_uint32(uint8_t * start);
uint16_t buf_to_uint16(uint8_t * start);

typedef struct wsg_data {
    uint8_t wsg_id; // 1 byte
    std::array<uint16_t, 3> sample; // 16 bit 2 byte * 3 = 6 bytes
    uint64_t timestamp; // 8 byte
    uint32_t dac_bias; // 4 byte
} wsg_data_t;

class SpiWsgRecv {
    public:
        SpiWsgRecv(SPIClass * spi_host_, uint8_t cs_pin_, SPISettings settings_);
        int recv(std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> * msg_buf);
    
    private:
        uint8_t cs_pin;
        SPIClass * spi_host;
        SPISettings spi_settings;

        wsg_data_t deserialize_message(uint8_t * start);

};

#endif