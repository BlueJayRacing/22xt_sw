idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES driver freertos log cxx
                    PRIV_REQUIRES ntp_udp_server spi_ntp udp_server)