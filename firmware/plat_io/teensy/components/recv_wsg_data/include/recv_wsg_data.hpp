#ifndef RECV_WSG_DATA_H
#define RECV_WSG_DATA_H

#include <Arduino.h>
#include <SPI.h>

#include <array>

#define MAX_MESSAGE_LEN 130
#define MESSAGES_PER_DATA_SEND 6
#define SERIALIZED_MSG_SIZE 19

struct wsg_data_t {
    uint8_t wsg_id;
    uint32_t dac_bias;
    uint16_t sample[3];
    uint64_t timestamp;
};

uint64_t buf_to_uint64(uint8_t * start);
uint32_t buf_to_uint32(uint8_t * start);
uint16_t buf_to_uint16(uint8_t * start);

class SpiWsgRecv {
    public:
        SpiWsgRecv::SpiWsgRecv(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_);
        int recv(std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> * msg_buf);
    
    private:
        uint8_t cs_pin;
        uint8_t handshake_pin; 
        SPIClass * spi_host;
        SPISettings spi_settings;

        wsg_data_t deserialize_message(uint8_t * start);
        
};

#endif