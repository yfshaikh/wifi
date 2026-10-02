idf_component_register(
        SRCS main.cc
        INCLUDE_DIRS ""
        REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip)