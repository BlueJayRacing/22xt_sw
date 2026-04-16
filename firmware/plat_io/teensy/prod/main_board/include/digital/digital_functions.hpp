#pragma once

#include "util/ring_buffer.hpp"
#include "util/circular_buffer.hpp"
#include "util/sample_data.hpp"
#include "config/config.hpp"
#include <Arduino.h>

namespace baja {
namespace digital {

// Constants for the digital input pins
constexpr uint8_t D1_PIN = 21;
constexpr uint8_t D2_PIN = 20;
constexpr uint8_t D3_PIN = 17;
constexpr uint8_t D4_PIN = 16;
constexpr uint8_t D5_PIN = 15;
constexpr uint8_t D6_PIN = 14;

static uint8_t digital_pins[6] = {0};

// Number of digital channels
constexpr uint8_t DIGITAL_CHANNEL_COUNT = 6;

// Internal channel IDs for digital channels (matching the mapping in teensy_mapping.hpp)
constexpr uint8_t DIGITAL_CHANNEL_ID_START = 16; // As per the mapping

namespace functions {

/**
 * @brief Initialize the digital input module
 * 
 * @param mainBuffer Reference to the main ring buffer for SD storage
 * @param fastBuffer Reference to the fast buffer for network transmission
 * @return true if initialization was successful
 */
bool initialize(
    buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer);

/**
 * @brief Start digital input monitoring
 * 
 * @return true if successful
 */
bool start();

/**
 * @brief Stop digital input monitoring
 * 
 * @return true if successful
 */
bool stop();

/**
 * @brief Check if digital input monitoring is running
 * 
 * @return true if running
 */
bool isRunning();

/**
 * @brief Process digital inputs - main function called in master loop
 * 
 * @return true if samples were processed
 */
bool process();

/**
 * @brief Get timing statistics for digital input processing
 * 
 * @param avgTime Average processing time in microseconds
 * @param minTime Minimum processing time in microseconds
 * @param maxTime Maximum processing time in microseconds
 * @param sampleCount Total samples processed
 */
void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint64_t& sampleCount);

/**
 * @brief Reset timing statistics
 */
void resetTimingStats();

/**
 * @brief Get the total sample count
 * 
 * @return Number of samples processed
 */
uint64_t getSampleCount();

} // namespace functions

} // namespace digital
} // namespace baja