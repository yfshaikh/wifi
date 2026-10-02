#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "../../_components/nvs_component.h"
#include "../../_components/config_component.h"
#include "../../_components/csi_component.h"
#include "../../_components/time_component.h"
#include "../../_components/input_component.h"
#include "../../_components/sockets_component.h"

static const char *TAG = "wifi-sensing-tx";

static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retry connecting to the AP");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool is_wifi_connected() {
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT);
}

void station_init() {
    const node_config_t *cfg = config_get();

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_sta_config_t wifi_sta_config = {};
    wifi_sta_config.channel = cfg->sta_channel;
    wifi_config_t wifi_config = {
            .sta = wifi_sta_config,
    };

    strlcpy((char *) wifi_config.sta.ssid, cfg->sta_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *) wifi_config.sta.password, cfg->sta_password, sizeof(wifi_config.sta.password));

    if (wifi_config.sta.channel == 0) {
        wifi_config.sta.channel = 0; /* 0 = auto-scan, otherwise the configured sensing channel */
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "connect to ap SSID:%s", cfg->sta_ssid);
}

void vTask_socket_transmitter_sta_loop(void *pvParameters) {
    for (;;) {
        socket_transmitter_sta_loop(&is_wifi_connected);
    }
}

void build_config_print() {
    printf("\n\n\n");
    printf("-----------------------\n");
    printf("wifi-sensing TX settings\n");
    printf("-----------------------\n");
    printf("IDF_VER: %s\n", IDF_VER);
    printf("-----------------------\n\n\n");
}

extern "C" void app_main() {
    build_config_print();
    nvs_init();
    config_init();
    config_print();
    station_init();
    csi_init((char *) "STA");

    /* Unpinned: the C6 has one core, so pinning to core 1 asserts. Priority 5
       keeps this loop below the WiFi (23) and lwIP tasks it depends on. */
    xTaskCreate(&vTask_socket_transmitter_sta_loop, "socket_transmitter_sta_loop",
                10000, NULL, 5, NULL);

    input_loop_init();
    printf("Type STASSID:/STAPASS:/STACH:/RATE:/SHOWCONFIG/REBOOT to configure.\n");
}