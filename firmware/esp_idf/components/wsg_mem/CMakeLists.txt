idf_component_register(
    SRCS "wsg_mem.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_http_client w25n04kv driver esp_system
)
