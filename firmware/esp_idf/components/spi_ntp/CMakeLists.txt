idf_component_register(SRCS "spi_ntp.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_spi esp_timer esp_driver_gpio)