#include <Arduino.h>
#include <SPI.h>
#include <spi_ntp.hpp>

#define MAX_ATTEMPTS 1000

SPISettings settings(1000000, MSBFIRST, SPI_MODE0);

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    Serial.printf("setup\n");
    
    SPI1.begin();
    SPI1.setMISO(39); 
}

void loop()
{
    // SPISettings settings(1000000, LSBFIRST, SPI_MODE0);
    // SPIClass* spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_
    NTPviaSPI sync(&SPI1, 36, 2, settings);
    if (sync.sync() == 0) {
        uint64_t now  = getMicrosecondsSinceEpoch();
        uint32_t sec  = (uint32_t)(now / 1000000ULL);
        uint32_t usec = (uint32_t)(now % 1000000ULL);
        Serial.printf("TEENSY Epoch: %u.%06u\n", sec, usec);
    } else {
        Serial.printf("Sync Failed\n");
    }
}