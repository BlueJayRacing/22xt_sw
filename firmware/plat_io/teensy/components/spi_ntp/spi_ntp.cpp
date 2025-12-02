#include "spi_ntp.hpp"

NTPviaSPI::NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_, SPISettings settings_) : spi_host(spi_host_), cs_pin(cs_pin_), spi_settings(settings_) {
    // do I even need to set this pin on esp?
    // pinMode(cs_pin, OUTPUT);
}

int32_t NTPviaSPI::sync() {
    // send msg telling esp to start sync

    // recv esp first msg log time
    struct timeval recv_time;

    // send back time sent
    struct timeval time_now;

}

