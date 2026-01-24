#include <Arduino.h>
#include <spi_ntp.hpp>
#include <SPI.h>

void setup() {

}

void loop() {
  SPISettings settings(10000000, MSBFIRST, SPI_MODE0);
  SPI1.begin();

  NTPviaSPI sync(&SPI1, 0, settings);
  sync.sync();
}