#include "recv_wsg_data.hpp"

uint64_t buf_to_uint64(uint8_t * start) {
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num += (uint64_t)*start << ((7-i) * 8);
        start++;
    }

    return num;
}

uint32_t buf_to_uint32(uint8_t * start) {
    uint32_t num = 0;
    for (int i = 0; i < 4; i++) {
        num += (uint32_t)*start << ((3-i) * 8);
        start++;
    }

    return num;
}

uint16_t buf_to_uint16(uint8_t * start) {
    uint16_t num = 0;
    for (int i = 0; i < 2; i++) {
        num += (uint16_t)*start << ((1-i) * 8);
        start++;
    }

    return num;
}

SpiWsgRecv::SpiWsgRecv(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) : spi_host(spi_host_), cs_pin(cs_pin_), handshake_pin(handshake_pin_), spi_settings(settings_) {
    // pinMode(cs_pin, OUTPUT);
    // pinMode(handshake_pin, INPUT); 
    // digitalWrite(cs_pin, HIGH);
}

SpiWsgRecv::SpiWsgRecv() {}

void SpiWsgRecv::init(SPIClass * spi_host_, uint8_t cs_pin_, uint8_t handshake_pin_, SPISettings settings_) {
    spi_host = spi_host_;
    cs_pin = cs_pin_;
    handshake_pin = handshake_pin_;
    spi_settings = settings_;
}

// id 8, dac bias 32, samples 3x16, timestamp 64
wsg_data_t SpiWsgRecv::deserialize_message(uint8_t * start) {
    wsg_data_t data;
    data.wsg_id = *start;

    start++;
    data.dac_bias = buf_to_uint32(start);
    
    start += 4;
    for(int i = 0; i < 3; i++) {
        data.sample[i] = buf_to_uint16(start);
        start += 2;    
    }

    data.timestamp = buf_to_uint64(start);

    return data;
}

// TODO: add error handling
int SpiWsgRecv::recv(std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> * msg_buf, bool next_iter_sync) {
    if(digitalRead(handshake_pin) == HIGH) {
        spi_host->beginTransaction(spi_settings);

        std::array<uint8_t, 1> send_buf = {0};
        if (next_iter_sync) {
            send_buf[0] = 0x88;
        } else {
            send_buf[0] = 0x15;
        }

        std::array<uint8_t, MAX_MESSAGE_LEN> ret_buf;

        digitalWrite(cs_pin, LOW);
        spi_host->transfer(send_buf.data(), ret_buf.data(), ret_buf.size());
        digitalWrite(cs_pin, HIGH);

        uint8_t * data_start = ret_buf.data();
        for (int i = 0; i < MESSAGES_PER_DATA_SEND; i++) {
            (*msg_buf)[i] = deserialize_message(data_start);
            data_start += SERIALIZED_MSG_SIZE;
        }

        spi_host->endTransaction();

        Serial.print("Received bytes: ");
        for (int i = 0; i < ret_buf.size(); i++) {
            Serial.print(ret_buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        return 1;
    }

    return 0;
}