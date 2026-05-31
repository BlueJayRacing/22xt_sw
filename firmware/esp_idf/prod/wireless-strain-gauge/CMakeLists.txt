# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/wsg_mem
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/w25n04kv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/ad5626 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/ads1120 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/udp_client
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/lock_guard
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/ntp_udp_client
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wireless-strain-guage)
