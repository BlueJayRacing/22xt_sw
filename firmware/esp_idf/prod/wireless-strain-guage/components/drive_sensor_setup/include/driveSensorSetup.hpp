#pragma once
#ifndef _DRIVE_SENSOR_SETUP_HPP_
#define _DRIVE_SENSOR_SETUP_HPP_

#include <ad5626.hpp>
#include <ads1120.hpp>
#include <esp_event.h>
#include <esp_system.h>

#include <array>

#define WSG_BYTE_LEN = 1

typedef struct drive_cfg
{ 
  enum mode_t { ZEROING_MODE, MEASURING_MODE } mode;
  enum channel_t {STRAIN_GAUGE_0, STRAIN_GAUGE_1, STRAIN_GAUGE_2 } channel;
} drive_cfg_t;

typedef struct wsg_data {
    uint8_t wsg_id; // 1 byte
    std::array<uint16_t, 3> sample; // 16 bit 2 byte * 3 = 6 bytes
    uint64_t timestamp; // 8 byte
    uint32_t dac_bias; // 4 byte
} wsg_data_t;


std::array<uint8_t, 2> uint16_to_arr(uint16_t val);
std::array<uint8_t, 4> uint32_to_arr(uint32_t val);
std::array<uint8_t, 8> uint64_to_arr(uint64_t val);

std::array<uint8_t, 19> serialize_wsg_data(wsg_data data);

typedef struct drive_measurement {
    float voltage;
    uint8_t gain;
    int16_t adc_value;
    uint16_t dac_bias;
} drive_measurement_t;

class driveSensorSetup {
    public:
      driveSensorSetup();
      esp_err_t init(ads1120_init_param_t adc_params, ad5626_init_param_t dac_params);
      esp_err_t zero(void);
      esp_err_t configure(drive_cfg_t new_cfg);
      esp_err_t measure(bool wait_ready, drive_measurement_t* measurement);
      esp_err_t setDACValue(uint16_t new_dac_value);

    private:
      AD5626 dac_;
      ADS1120 adc_;
      drive_cfg_t cfg_;
};

#endif