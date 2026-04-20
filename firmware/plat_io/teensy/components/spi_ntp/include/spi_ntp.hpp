#ifndef SPI_NTP_H

#include <Arduino.h>
#include <SPI.h>

#include <array>

#define MAX_ATTEMPTS 10

uint64_t getMicrosecondsSinceEpoch();

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf);
std::array<uint8_t, 8> uint64_to_buf(uint64_t num);

class NTPviaSPI {
    public:
        NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_);
        NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_);
        int32_t sync();
    
    private:
        uint8_t cs_pin;
        uint8_t handshake_pin; 
        SPIClass * spi_host;
        SPISettings spi_settings;

};

#endif