#include "spi_ntp.hpp"

uint64_t getMicrosecondsSinceEpoch()
{
    uint32_t hi = SNVS_HPRTCMR;
    uint32_t lo = SNVS_HPRTCLR;
    // The RTC seconds are formed as: seconds = (hi << 17) | (lo >> 15)
    uint32_t secs = (hi << 17) | (lo >> 15);
    // The fractional part (ticks within the second) are the lower 15 bits.
    uint32_t frac = lo & 0x7FFF;
    // Convert to microseconds.
    return ((uint64_t)secs * 1000000ULL) + (((uint64_t)frac * 1000000ULL) / 32768);
}

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf)
{
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num += buf.at(8) << (i * 8);
    }

    return num;
}

std::array<uint8_t, 8> uint64_to_buf(uint64_t num)
{
    std::array<uint8_t, 8> buf;
    for (int i = 0; i < 8; i++) {
        buf[i] = (num >> (i * 8)) & 0xFF;
    }

    return buf;
}

NTPviaSPI::NTPviaSPI(SPIClass* spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_) : spi_host(spi_host_), cs_pin(cs_pin_), handshake_pin(handshake_pin_)
{
    // do I even need to set this pin on esp?
    pinMode(cs_pin, OUTPUT);
    pinMode(handshake_pin, INPUT); 
    digitalWrite(cs_pin, HIGH);
}

NTPviaSPI::NTPviaSPI(SPIClass* spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) : spi_host(spi_host_), cs_pin(cs_pin_), handshake_pin(handshake_pin_), spi_settings(settings_)
{
    pinMode(cs_pin, OUTPUT);
    pinMode(handshake_pin, INPUT); 
    digitalWrite(cs_pin, HIGH);
}

NTPviaSPI::NTPviaSPI() {}

void NTPviaSPI::init(SPIClass* spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) {
    spi_host = spi_host_;
    cs_pin = cs_pin_;
    handshake_pin = handshake_pin_;
    spi_settings = settings_;

    pinMode(cs_pin, OUTPUT);
    pinMode(handshake_pin, INPUT); 
    digitalWrite(cs_pin, HIGH);
}

int32_t NTPviaSPI::sync()
{
    // send msg telling esp to start sync
    Serial.printf("sync begin\n");

    // setup spi

    // std::array<uint8_t, 8> send_buf = {0x00, 1, 0, 1, 0, 0, 0, 0};
    // std::array<uint8_t, 8> ret_buf = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t send_buf[8] = {2, 1, 0, 1, 0, 0, 0, 2};
    uint8_t ret_buf[8]  = {0};

    uint8_t attempts = 0;

    spi_host->beginTransaction(spi_settings);

    Serial.printf("setup spi\n");

    // check if esp is up
    do {
        Serial.println("check if esp is up");
        while(digitalRead(handshake_pin) == LOW) {
            // Serial.println(digitalRead(handshake_pin));
        }
        digitalWrite(cs_pin, LOW);
        Serial.println("starting transfer");
        spi_host->transfer(send_buf, ret_buf, 8);
        // spi_host->transfer(ret_buf.data(), 8);
        digitalWrite(cs_pin, HIGH);
        delay(100);
        Serial.printf("%d ", ret_buf[0]);
        Serial.printf("%d ", ret_buf[1]);
        Serial.printf("%d ", ret_buf[2]);
        Serial.printf("%d ", ret_buf[3]);
        Serial.printf("%d ", ret_buf[4]);
        Serial.printf("%d ", ret_buf[5]);
        Serial.printf("%d ", ret_buf[6]);
        Serial.printf("%d\n", ret_buf[7]);
        attempts++;
        // delay(2000);
    } while (ret_buf[0] != 0x01 && attempts < MAX_ATTEMPTS);

    Serial.printf("attempts: %d\n", attempts);
    
    // err if reached max attempts
    if (attempts >= MAX_ATTEMPTS) {
        Serial.printf("ESP32 failed to respond\n");
        spi_host->endTransaction();
        return -1; // Return error
    }

    // first message
    Serial.printf("first message\n");

    // trans[0] receive request, record t1
    while(digitalRead(handshake_pin) == LOW);
    digitalWrite(cs_pin, LOW);
    spi_host->transfer(send_buf, ret_buf, 8);
    uint64_t t1 = getMicrosecondsSinceEpoch();  
    digitalWrite(cs_pin, HIGH);
    while(digitalRead(handshake_pin) == HIGH); 
    while(digitalRead(handshake_pin) == LOW);

    // trans[1] send t1 back, record t2
    uint64_t t2 = getMicrosecondsSinceEpoch();  
    std::array<uint8_t, 8> t1_buf = uint64_to_buf(t1);
    while(digitalRead(handshake_pin) == LOW);
    digitalWrite(cs_pin, LOW);
    spi_host->transfer(t1_buf.data(), ret_buf, 8);
    digitalWrite(cs_pin, HIGH);
    while(digitalRead(handshake_pin) == HIGH); 
    while(digitalRead(handshake_pin) == LOW);

    // trans[2] send t2 back
    std::array<uint8_t, 8> t2_buf = uint64_to_buf(t2);
    while(digitalRead(handshake_pin) == LOW);
    digitalWrite(cs_pin, LOW);
    spi_host->transfer(t2_buf.data(), ret_buf, 8);
    digitalWrite(cs_pin, HIGH);
    
    spi_host->endTransaction();

    Serial.printf("Timestamp own: %llu\n", getMicrosecondsSinceEpoch());

    return 0;
}
