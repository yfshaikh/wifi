idf_component_register(
        SRCS main.cc
        INCLUDE_DIRS ""
        REQUIRES mqtt esp_timer esp_wifi esp_event esp_netif nvs_flash lwip)