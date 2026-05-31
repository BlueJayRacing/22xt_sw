#include "digital/digital_functions.hpp"
#include "util/debug_util.hpp"
#include "util/time_since_epoch.hpp" // For high-precision timestamps

namespace baja {
namespace digital {
namespace functions {

// Static variables to maintain state
static bool running_ = false;
static uint64_t sampleCount_ = 0;
static buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>* mainBuffer_ = nullptr;
static buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>* fastBuffer_ = nullptr;

// Timing statistics
static uint32_t totalProcessingTime_ = 0;
static uint32_t minProcessingTime_ = UINT32_MAX;
static uint32_t maxProcessingTime_ = 0;
static uint32_t processingCount_ = 0;
static uint32_t lastStatResetTime_ = 0;

// Last sample time for each channel
static uint32_t lastSampleTimeMs_[DIGITAL_CHANNEL_COUNT] = {0};
static const uint32_t SAMPLE_INTERVAL_MS = 10; // sample at least every 10 ms

// Volatile counters for each digital input
static volatile uint32_t digitalCounters_[DIGITAL_CHANNEL_COUNT] = {0};
static volatile bool digital_prev[DIGITAL_CHANNEL_COUNT] = {false};

// Flags indicating if a counter has been incremented since the last sampling
static volatile bool digitalCounterIncremented_[DIGITAL_CHANNEL_COUNT] = {false};

// Interrupt service routines for each digital input
FASTRUN static void isr_d1() {if (!digitalCounterIncremented_[0]) digitalCounters_[0]++; digitalCounterIncremented_[0] = true; } // if (!digitalCounterIncremented_[0]) 
FASTRUN static void isr_d2() {if (!digitalCounterIncremented_[1]) digitalCounters_[1]++; digitalCounterIncremented_[1] = true; } // if (!digitalCounterIncremented_[1]) 
FASTRUN static void isr_d3() {if (!digitalCounterIncremented_[2]) digitalCounters_[2]++; digitalCounterIncremented_[2] = true; } // if (!digitalCounterIncremented_[2]) 
FASTRUN static void isr_d4() {if (!digitalCounterIncremented_[3]) digitalCounters_[3]++; digitalCounterIncremented_[3] = true; } // if (!digitalCounterIncremented_[3]) 
FASTRUN static void isr_d5() {if (!digitalCounterIncremented_[4]) digitalCounters_[4]++; digitalCounterIncremented_[4] = true; } // if (!digitalCounterIncremented_[4]) 
FASTRUN static void isr_d6() {if (!digitalCounterIncremented_[5]) digitalCounters_[5]++; digitalCounterIncremented_[5] = true; } // if (!digitalCounterIncremented_[5]) 

bool initialize(
    buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer) {
    
    util::Debug::info(F("Digital: Initializing"));
    
    // Store the buffer references
    mainBuffer_ = &mainBuffer;
    fastBuffer_ = &fastBuffer;

    digital_pins[0] = D1_PIN;
    digital_pins[1] = D2_PIN;
    digital_pins[2] = D3_PIN;
    digital_pins[3] = D4_PIN;
    digital_pins[4] = D5_PIN;
    digital_pins[5] = D6_PIN;

    // Initialize counters and flags
    for (int i = 0; i < DIGITAL_CHANNEL_COUNT; i++) {
        digitalCounters_[i] = 0;
        digitalCounterIncremented_[i] = false;
        lastSampleTimeMs_[i] = 0;
        digital_prev[i] = false;
    }
    
    // Configure pins as inputs
    pinMode(D1_PIN, INPUT);
    pinMode(D2_PIN, INPUT);
    pinMode(D3_PIN, INPUT);
    pinMode(D4_PIN, INPUT);
    pinMode(D5_PIN, INPUT);
    pinMode(D6_PIN, INPUT);
    
    // Reset timing statistics
    resetTimingStats();
    
    util::Debug::info(F("Digital: Initialization successful"));
    return true;
}

bool start() {
    // Check if already running
    if (running_) {
        util::Debug::warning(F("Digital: Already running"));
        return true;
    }
    
    util::Debug::info(F("Digital: Attaching interrupts"));
    
    // Attach interrupts for each digital input on RISING edge
    attachInterrupt(digitalPinToInterrupt(D1_PIN), isr_d1, RISING);
    attachInterrupt(digitalPinToInterrupt(D2_PIN), isr_d2, RISING);
    attachInterrupt(digitalPinToInterrupt(D3_PIN), isr_d3, RISING);
    attachInterrupt(digitalPinToInterrupt(D4_PIN), isr_d4, RISING);
    attachInterrupt(digitalPinToInterrupt(D5_PIN), isr_d5, RISING);
    attachInterrupt(digitalPinToInterrupt(D6_PIN), isr_d6, RISING);
    
    // Reset sample count
    sampleCount_ = 0;
    
    running_ = true;
    util::Debug::info(F("Digital: Started"));
    return true;
}

bool stop() {
    if (!running_) {
        return true;
    }
    
    // Detach interrupts
    detachInterrupt(digitalPinToInterrupt(D1_PIN));
    detachInterrupt(digitalPinToInterrupt(D2_PIN));
    detachInterrupt(digitalPinToInterrupt(D3_PIN));
    detachInterrupt(digitalPinToInterrupt(D4_PIN));
    detachInterrupt(digitalPinToInterrupt(D5_PIN));
    detachInterrupt(digitalPinToInterrupt(D6_PIN));
    
    running_ = false;
    util::Debug::info(F("Digital: Stopped"));
    
    return true;
}

bool isRunning() {
    return running_;
}

bool process() {
    // Check if digital monitoring is running
    if (!running_ || !mainBuffer_ || !fastBuffer_) {
        util::Debug::info("mem inssue in digital");
        return false;
    }
    
    // Start timing for this operation
    uint32_t startTime = micros();
    
    // Flag to track if any samples were processed
    bool samplesProcessed = false;
    
    // Current time
    uint32_t currentTimeMs = millis();
    
    // Check each digital input
    for (int i = 0; i < DIGITAL_CHANNEL_COUNT; i++) {
        // Check if counter was incremented or if sampling interval has elapsed
        // We need to disable interrupts temporarily for reading the volatile flag
        noInterrupts();
        // bool is_high = digitalRead(digital_pins[i]) == HIGH;
        // if (true || i == 0) {
        //     util::Debug::info(F("Channel ") + String(i) + F(" is high: ") + String(is_high));
        // }
        bool needSample = digitalCounterIncremented_[i] || (currentTimeMs - lastSampleTimeMs_[i] >= SAMPLE_INTERVAL_MS);

        if (needSample) {
            // // Read the counter value
            // if(is_high && !digital_prev[i]) {
            //     digitalCounters_[i]++;
            // }

            // digital_prev[i] = is_high;

            // if (!(currentTimeMs - lastSampleTimeMs_[i] >= SAMPLE_INTERVAL_MS)) continue;

            uint32_t counterValue = digitalCounters_[i];
            // if (counterValue != 0) {
            //     util::Debug::info(F("Got count: ") + String(counterValue) + F(" on channel: ") + String(i));
            // }

            if (digitalCounterIncremented_[i]) {
                util::Debug::info(F("Digital channel: ") + String(i) + F(" with count: ") + String(counterValue));
            }
            
            // Reset the incremented flag
            digitalCounterIncremented_[i] = false;
            
            // Re-enable interrupts
            interrupts();
            
            // Update last sample time
            lastSampleTimeMs_[i] = currentTimeMs;
            
            // Get a high-precision timestamp for this sample
            uint64_t timestampMicros = getMicrosecondsSinceEpoch();
            
            // Create a channel sample with internal ID and timestamp
            data::ChannelSample channelSample(
                timestampMicros,                // Microsecond timestamp
                DIGITAL_CHANNEL_ID_START + i,   // Internal channel ID (16-21)
                counterValue,                   // Raw counter value
                currentTimeMs                   // Recorded time in milliseconds
            );
            
            // Write to main buffer for SD storage
            bool mainBufferOk = mainBuffer_->write(channelSample);
            
            // Also write to fast buffer for network transmission
            // Note: Using a static counter for downsampling, similar to what might be in ADC code
            static uint32_t downsampleCounter = 0;
            bool fastBufferOk = false;
            
            // Only add to fast buffer according to downsample ratio
            if (downsampleCounter % config::FAST_BUFFER_DOWNSAMPLE_RATIO == 0) {
                fastBufferOk = fastBuffer_->write(channelSample);
                
                // Log warning if fast buffer is full (but not too frequently)
                if (!fastBufferOk) {
                    static uint32_t lastFastBufferFullWarning = 0;
                    if (currentTimeMs - lastFastBufferFullWarning > 5000) { // Only warn every 5 seconds
                        util::Debug::warning(F("Digital: Fast buffer full, sample dropped"));
                        lastFastBufferFullWarning = currentTimeMs;
                    }
                }
            }
            downsampleCounter++;
            
            if (mainBufferOk) {
                samplesProcessed = true;
                sampleCount_++;
            } else {
                // Main buffer full, log warning occasionally
                static uint32_t lastBufferFullWarning = 0;
                if (currentTimeMs - lastBufferFullWarning > 5000) { // Only warn every 5 seconds
                    util::Debug::warning(F("Digital: Main buffer full, digital sample dropped"));
                    lastBufferFullWarning = currentTimeMs;
                }
            }
        } else {
            // Re-enable interrupts if we didn't need to sample
            interrupts();
        }
    }
    
    // Calculate processing time if we actually did work
    if (samplesProcessed) {
        uint32_t processingTime = micros() - startTime;
        
        // Update statistics
        totalProcessingTime_ += processingTime;
        processingCount_++;
        
        if (processingTime < minProcessingTime_) {
            minProcessingTime_ = processingTime;
        }
        
        if (processingTime > maxProcessingTime_) {
            maxProcessingTime_ = processingTime;
        }
    }
    
    return samplesProcessed;
}

void getTimingStats(float& avgTime, uint32_t& minTime, uint32_t& maxTime, uint64_t& sampleCount) {
    avgTime = processingCount_ > 0 ? (float)totalProcessingTime_ / processingCount_ : 0.0f;
    minTime = minProcessingTime_ == UINT32_MAX ? 0 : minProcessingTime_;
    maxTime = maxProcessingTime_;
    sampleCount = sampleCount_;
}

void resetTimingStats() {
    totalProcessingTime_ = 0;
    minProcessingTime_ = UINT32_MAX;
    maxProcessingTime_ = 0;
    processingCount_ = 0;
    lastStatResetTime_ = millis();
}

uint64_t getSampleCount() {
    return sampleCount_;
}

} // namespace functions
} // namespace digital
} // namespace baja