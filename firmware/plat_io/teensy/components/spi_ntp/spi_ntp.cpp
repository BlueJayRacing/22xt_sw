#include "spi_ntp.hpp"

uint64_t getMicrosecondsSinceEpoch() {
    uint32_t hi = SNVS_HPRTCMR;
    uint32_t lo = SNVS_HPRTCLR;
    // The RTC seconds are formed as: seconds = (hi << 17) | (lo >> 15)
    uint32_t secs = (hi << 17) | (lo >> 15);
    // The fractional part (ticks within the second) are the lower 15 bits.
    uint32_t frac = lo & 0x7FFF;
    // Convert to microseconds.
    return ((uint64_t)secs * 1000000ULL) + (((uint64_t)frac * 1000000ULL) / 32768);
}

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf) {
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num += buf.at(8) << (i * 8);
    }

    return num;
}

std::array<uint8_t, 8> uint64_to_buf(uint64_t num) {
    std::array<uint8_t, 8> buf;
    for (int i = 0; i < 8; i++) {
        buf[i] = (num >> (i * 8)) & 0xFF;
    }

    return buf;
}

NTPviaSPI::NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_) : spi_host(spi_host_), cs_pin(cs_pin_) {
    // do I even need to set this pin on esp?
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    spi_settings = SPISettings(10000000, MSBFIRST, SPI_MODE0);
}

NTPviaSPI::NTPviaSPI(SPIClass * spi_host_, uint8_t cs_pin_, SPISettings settings_) : spi_host(spi_host_), cs_pin(cs_pin_), spi_settings(settings_) {
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);
}

int32_t NTPviaSPI::sync() {
    // send msg telling esp to start sync
    Serial.printf("sync begin\n"); 

    // setup spi
    spi_host->beginTransaction(spi_settings);

    std::array<uint8_t, 8> send_buf = {0x00, 0, 0, 0, 0, 0, 0, 0};
    std::array<uint8_t, 8> ret_buf;

    uint8_t attempts = 0;

    Serial.printf("setup spi\n"); 

    // check if esp is up
    do {
        Serial.printf("check if esp is up\n"); 
        digitalWrite(cs_pin, LOW);
        spi_host->transfer(send_buf.data(), ret_buf.data(), 8);
        digitalWrite(cs_pin, HIGH);
        delay(100);
        
        if (ret_buf[0] == 0x01) break; 
        
    } while (++attempts < MAX_ATTEMPTS);

    if (attempts >= MAX_ATTEMPTS) {
        Serial.printf("ESP32 failed to respond\n");
        spi_host->endTransaction();
        return -1; // Return error
    }
    
    // err if reached max attempts

    // first message
    Serial.printf("first message\n"); 
    uint64_t t1 = getMicrosecondsSinceEpoch();
    digitalWrite(cs_pin, LOW);
    spi_host->transfer(send_buf.data(), ret_buf.data(), 8);
    digitalWrite(cs_pin, HIGH);

    delay(2);

    digitalWrite(cs_pin, LOW);
    uint64_t t2 = getMicrosecondsSinceEpoch();
    std::array<uint8_t, 8> t2_send_buf = uint64_to_buf(t2);
    spi_host->transfer(t2_send_buf.data(), ret_buf.data(), 8);
    digitalWrite(cs_pin, HIGH);

    delay(2);

    // send t1
    digitalWrite(cs_pin, LOW);
    std::array<uint8_t, 8> t1_send_buf = uint64_to_buf(t1);
    spi_host->transfer(t1_send_buf.data(), ret_buf.data(), 8);
    digitalWrite(cs_pin, HIGH);

    spi_host->endTransaction();

    return 0; 
}

