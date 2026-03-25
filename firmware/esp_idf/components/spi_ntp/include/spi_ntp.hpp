#pragma once
#ifndef _SPI_NTP_HPP_
#define _SPI_NTP_HPP_

#include "esp_attr.h"
#include <array>
#include <cstdint>
#include <driver/spi_slave.h>
#include <sys/time.h>

void recv_time(spi_slave_transaction_t* trans);

class NTPviaSPI {
  public:
    NTPviaSPI(spi_host_device_t host);
    esp_err_t sync();
    uint8_t recv_count = 0;
    struct timeval t0;
    struct timeval t3;
    esp_err_t recv_time_err = ESP_OK;

  private:
    struct timeval t1;
    struct timeval t2;

    spi_host_device_t spi_host;

    spi_slave_transaction_t* trans[4];
    WORD_ALIGNED_ATTR std::array<uint8_t, 8> rx_buf[3];
    WORD_ALIGNED_ATTR std::array<uint8_t, 8> dummy_tx_buf = {0x01, 0, 0, 0, 0, 0, 0, 0};
};

#endif