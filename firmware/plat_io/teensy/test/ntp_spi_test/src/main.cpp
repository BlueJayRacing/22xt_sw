#include <Arduino.h>
#include <spi_ntp.hpp>
#include <SPI.h>

#define MAX_ATTEMPTS 1000

void setup() {
  Serial.begin(115200); 
  Serial.printf("setup\n");
  SPI2.begin();
}

void loop() {
  SPISettings settings(10000000, MSBFIRST, SPI_MODE0);
  
  NTPviaSPI sync(&SPI2, 0, settings);
  if (sync.sync() == 0) {
      uint64_t now = getMicrosecondsSinceEpoch();
      uint32_t sec = (uint32_t)(now / 1000000ULL);
      uint32_t usec = (uint32_t)(now % 1000000ULL);
      Serial.printf("TEENSY Epoch: %u.%06u\n", sec, usec);
  } else {
      Serial.printf("Sync Failed\n");
  }

  delay(2000);
}