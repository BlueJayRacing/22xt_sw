#ifndef SPI_NTP

#include <Arduino.h>
#include <SPI.h>

class NTPviaSPI {
    public:
        NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_, SPISettings settings_);
        ~NTPviaSPI();
    
    private:
        SPIClass * spi_host;
        SPISettings spi_settings;
        uint8_t cs_pin;

};

#endif SPI_NTP