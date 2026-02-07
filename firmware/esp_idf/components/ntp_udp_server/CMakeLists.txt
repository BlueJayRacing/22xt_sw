idf_component_register( SRCS "ntp_udp_server.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "esp_wifi" "esp_http_server" "esp_http_client" "nvs_flash" "driver"
                    )