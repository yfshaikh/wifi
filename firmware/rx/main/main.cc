#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "../../_components/nvs_component.h"
#include "../../_components/config_component.h"
#include "../../_components/csi_component.h"
#include "../../_components/mqtt_component.h"
#include "../../_components/time_component.h"
#include "../../_components/input_component.h"

#define MAX_STA_CONN 4 /* TX + hub laptop, with headroom; C6 caps at 15 */

static const char *TAG = "wifi-sensing-rx";

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void softap_init() {
    const node_config_t *cfg = config_get();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfgw = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfgw));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_ap_config_t wifi_ap_config = {};
    wifi_ap_config.channel = cfg->ap_channel;
    wifi_ap_config.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_ap_config.max_connection = MAX_STA_CONN;

    wifi_config_t wifi_config = {
            .ap = wifi_ap_config,
    };

    strlcpy((char *) wifi_config.ap.ssid, cfg->ap_ssid, sizeof(wifi_config.ap.ssid));
    strlcpy((char *) wifi_config.ap.password, cfg->ap_password, sizeof(wifi_config.ap.password));

    if (strlen(cfg->ap_password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    /* No 802.11ax, 20 MHz: the TX's packets then arrive as HT20 PPDUs, so every
       CSI record is the same HT-LTF shape (see csi_init). */
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "softap_init finished. SSID:%s channel:%d", cfg->ap_ssid, cfg->ap_channel);
}

void build_config_print() {
    printf("\n\n\n");
    printf("-----------------------\n");
    printf("wifi-sensing RX settings\n");
    printf("-----------------------\n");
    printf("IDF_VER: %s\n", IDF_VER);
    printf("CONFIG_ESP_CONSOLE_UART_BAUDRATE: %d\n", CONFIG_ESP_CONSOLE_UART_BAUDRATE);
#if defined(CONFIG_SHOULD_COLLECT_CSI)
    printf("SHOULD_COLLECT_CSI: 1\n");
#else
    printf("SHOULD_COLLECT_CSI: 0\n");
#endif
#if defined(CONFIG_SEND_CSI_TO_MQTT)
    printf("SEND_CSI_TO_MQTT: 1\n");
#else
    printf("SEND_CSI_TO_MQTT: 0\n");
#endif
    printf("-----------------------\n\n\n");
}

extern "C" void app_main() {
    build_config_print();
    nvs_init();
    config_init();
    config_print();
    softap_init();

#if !defined(CONFIG_SHOULD_COLLECT_CSI)
    printf("CSI will not be collected. Check `menuconfig  # > ESP32 CSI Tool Config` to enable CSI");
#else
    csi_init((char *) "AP");
    mqtt_init();
#endif

    input_loop_init();
    printf("Type NODEID:/MQTTHOST:/APSSID:/APPASS:/APCH:/SHOWCONFIG/REBOOT to configure.\n");
}