/**
 * @file channel_mapping.hpp
 * @brief Utilities for mapping between ADC channel indices and semantic names (header-only)
 */

 #pragma once

 #include <string>
 #include <array>
 #include <unordered_map>
 
 namespace baja {
 namespace util {
 
 /**
  * @brief Enumeration of semantic channel names for ADC inputs
  */
 enum class ChannelName {
     GND,            // ADC AIN 0  - Ground reference
     SG2,            // ADC AIN 1  - strain gage 2
     SG1,            // ADC AIN 2  - strain gage 1
     TWO_5_LDO,      // ADC AIN 3  - 2.5V reference (buffered)
     
     // New channel names based on Squid
     LIN_POT_1_FL,   // ADC AIN 4  - Channel 1, linpot 1
     CHANNEL_2,      // ADC AIN 5  - Channel 2
     LIN_POT_2_FR,   // ADC AIN 6  - Channel 3, linpot 2
     STEERING_POT_1, // ADC AIN 7  - Channel 4, steering pot option 1
     CHANNEL_5,      // ADC AIN 8  - Channel 5
     LIN_POT_3_RL,   // ADC AIN 9  - Channel 6, linpot 3
     CHANNEL_7,      // ADC AIN 10 - Channel 7
     LIN_POT_4_RR,   // ADC AIN 11 - Channel 8, linpot 4
     STEERING_POT_2, // ADC AIN 12 - Channel 9, steering pot option 2
     CHANNEL_10,     // ADC AIN 13 - Channel 10
     MONITORING_V,   // ADC AIN 14 - monitor 5v should be 4.35
     TWO_5_REF,      // ADC AIN 15 - 2.5 v references
     ESP_3V3,        // ADC AIN 16 - esp 3v3
     UNKNOWN         // For invalid mappings

     // Old channel names based on KiCad
    //  LIN_POT_1_FL,   // ADC AIN 4 - linpot 1, FL
    //  LIN_POT_2_FR,   // ADC AIN 5 - linpot 2, FR
    //  LIN_POT_3_RL,   // ADC AIN 6 - linpot 3, RL
    //  LIN_POT_4_RR,   // ADC AIN 7 - linpot 4, RR
    //  STEERING_POT,   // ADC AIN 8 - steering linpot
    //  CHANNEL_6,      // ADC AIN 9 - Channel 6
    //  CHANNEL_7,      // ADC AIN 10 - Channel 7
    //  CHANNEL_8,      // ADC AIN 11 - Channel 8
    //  CHANNEL_9,      // ADC AIN 12 - Channel 9
    //  CHANNEL_10,     // ADC AIN 13 - Channel 10
    //  MONITORING_V,   // ADC AIN 14 - monitor 5v should be 4.35
    //  TWO_5_REF,      // ADC AIN 15 - 2.5 v references
    //  ESP_3V3,        // ADC AIN 16 - esp 3v3
    //  UNKNOWN         // For invalid mappings
 };
 
 // Static array of all channel names in their index order
 static const std::array<ChannelName, 18> ALL_CHANNEL_NAMES = {
     ChannelName::GND,            
     ChannelName::SG2,            
     ChannelName::SG1,            
     ChannelName::TWO_5_LDO,      
     ChannelName::LIN_POT_1_FL, 
     ChannelName::CHANNEL_2,  
     ChannelName::LIN_POT_2_FR,   
     ChannelName::STEERING_POT_1,  
     ChannelName::CHANNEL_5,  
     ChannelName::LIN_POT_3_RL,   
     ChannelName::CHANNEL_7,    
     ChannelName::LIN_POT_4_RR,   
     ChannelName::STEERING_POT_2,      
     ChannelName::CHANNEL_10,     
     ChannelName::MONITORING_V,   
     ChannelName::TWO_5_REF,      
     ChannelName::ESP_3V3,        
     ChannelName::UNKNOWN   
 };
 
 // String representations of channel names
 const std::unordered_map<ChannelName, std::string> CHANNEL_NAME_STRINGS = {
     {ChannelName::GND, "GND"},
     {ChannelName::TWO_5_LDO, "2.5V ldo"},
     {ChannelName::SG2, "SG2"},
     {ChannelName::SG1, "SG1"},
     {ChannelName::LIN_POT_1_FL, "front left linpot 1"},
     {ChannelName::LIN_POT_2_FR, "front right linpot 2"},
     {ChannelName::LIN_POT_3_RL, "rear left linpot 3"},
     {ChannelName::LIN_POT_4_RR, "rear right linpot 4"},
     {ChannelName::STEERING_POT_1, "steering pot option 1"},
     {ChannelName::STEERING_POT_2, "steering pot option 2"},
     {ChannelName::CHANNEL_2, "Channel_2"},
     {ChannelName::CHANNEL_5, "Channel_5"},
     {ChannelName::CHANNEL_7, "Channel_7"},
     {ChannelName::CHANNEL_10, "Channel_10"},
     {ChannelName::MONITORING_V, "5V level monitor"},
     {ChannelName::TWO_5_REF, "2.5V_Ref"},
     {ChannelName::ESP_3V3, "esp 3v3"},
     {ChannelName::UNKNOWN, "Unknown"} 
 };
 
 /**
  * @brief Get the semantic name for an ADC channel index
  * @param channelIndex The ADC channel index (0-15)
  * @return The corresponding ChannelName enum value
  */
//  inline ChannelName getChannelNameFromIndex(uint8_t channelIndex) {
//      if (channelIndex < ALL_CHANNEL_NAMES.size()) {
//          return ALL_CHANNEL_NAMES[channelIndex];
//      }
//      return ChannelName::UNKNOWN;
//  }
 
 /**
  * @brief Get the ADC channel index for a channel name
  * @param channelName The ChannelName enum value
  * @return The corresponding ADC channel index (0-15)
  */
//  inline uint8_t getIndexFromChannelName(ChannelName channelName) {
//      for (uint8_t i = 0; i < ALL_CHANNEL_NAMES.size(); i++) {
//          if (ALL_CHANNEL_NAMES[i] == channelName) {
//              return i;
//          }
//      }
//      return UINT8_MAX; // Invalid index
//  }
 
 /**
  * @brief Get the string representation of a channel name
  * @param channelName The ChannelName enum value
  * @return String representation of the channel name
  */
 inline std::string getChannelNameString(ChannelName channelName) {
     auto it = CHANNEL_NAME_STRINGS.find(channelName);
     if (it != CHANNEL_NAME_STRINGS.end()) {
         return it->second;
     }
     return "Unknown";
 }
 
 /**
  * @brief Get the string representation of a channel name from an index
  * @param channelIndex The ADC channel index (0-15)
  * @return String representation of the channel name
  */
//  inline std::string getChannelNameString(uint8_t channelIndex) {
//      return getChannelNameString(getChannelNameFromIndex(channelIndex));
//  }
 
 /**
  * @brief Check if a channel should be enabled by default
  * 
  * This function determines if a channel should be enabled by default
  * based on its semantic role (e.g., reference channels, sensor channels)
  * 
  * @param channelName The ChannelName enum value
  * @return true if the channel should be enabled by default, false otherwise
  */
 inline bool shouldADCChannelBeEnabled(ChannelName channelName) {
     // Enable all data channels by default, but not reference channels
     switch (channelName) {
         case ChannelName::GND:
         case ChannelName::ESP_3V3:
         case ChannelName::SG1:
         case ChannelName::SG2:
             return false; // Reference channels disabled by default
         case ChannelName::LIN_POT_1_FL:
         case ChannelName::LIN_POT_2_FR:
         case ChannelName::LIN_POT_3_RL:
         case ChannelName::LIN_POT_4_RR:
         case ChannelName::STEERING_POT_1:
         case ChannelName::STEERING_POT_2:
         case ChannelName::TWO_5_REF:
             return true; // Data channels enabled by default
         default:
             return false;
     }
 }
 
 /**
  * @brief Get all available channel names
  * @return Array of all channel names
  */
 inline const std::array<ChannelName, 18>& getAllChannelNames() {
     return ALL_CHANNEL_NAMES;
 }
 
 } // namespace util
 } // namespace baja