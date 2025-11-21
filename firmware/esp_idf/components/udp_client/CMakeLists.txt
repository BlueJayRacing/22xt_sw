idf_component_register( SRCS "udp_client.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES 
                            driver
                            mqtt
                            lock_guard
                            esp_wifi
                            esp_system
                            esp_netif
                            esp_event
                            esp_http_server
                            esp_http_client
                            esp_timer 
                            nvs_flash
                            freertos
                            log
                            )