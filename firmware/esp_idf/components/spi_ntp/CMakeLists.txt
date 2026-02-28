idf_component_register(SRCS "spi_ntp.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_spi esp_timer)