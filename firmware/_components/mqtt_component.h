#ifndef WIFI_SENSING_MQTT_COMPONENT_H
#define WIFI_SENSING_MQTT_COMPONENT_H

/*
 * Only the receiver/AP sets CONFIG_SEND_CSI_TO_MQTT. When it is not defined
 * (e.g. on the transmitter) this whole component compiles to nothing, so the
 * transmitter doesn't need the ESP-IDF `mqtt` component linked in.
 */
#ifdef CONFIG_SEND_CSI_TO_MQTT

#include <stdio.h>
#include <string.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "config_component.h"

static const char *MQTT_TAG = "mqtt";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static volatile bool s_mqtt_connected = false;
static char s_mqtt_topic[160];

/*
 * CSI callback -> ring buffer -> publisher task -> broker.
 * The callback runs in the WiFi driver task, which must never block on a TCP
 * send, so it only copies the record in here (dropping it when full). The
 * publisher task does the blocking publish at full rate. (esp-mqtt's own
 * enqueue path sends one queued message per ~1 s poll loop, so it can't keep
 * up with 100 Hz.) 32 KB is ~0.5 s of backlog; beyond that we drop, never buffer.
 */
#define MQTT_RINGBUF_BYTES (32 * 1024)
static RingbufHandle_t s_ringbuf = NULL;

static void _mqtt_publish_task(void *arg) {
    for (;;) {
        size_t len;
        char *msg = (char *) xRingbufferReceive(s_ringbuf, &len, portMAX_DELAY);
        if (s_mqtt_connected) {
            esp_mqtt_client_publish(s_mqtt_client, s_mqtt_topic, msg, len, 0, 0);
        }
        vRingbufferReturnItem(s_ringbuf, msg);
    }
}

static void _mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_id_t event = (esp_mqtt_event_id_t) event_id;
    switch (event) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_connected = true;
            ESP_LOGI(MQTT_TAG, "connected; publishing to %s", s_mqtt_topic);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_connected = false;
            ESP_LOGW(MQTT_TAG, "disconnected; will retry");
            break;
        default:
            break;
    }
}

void mqtt_init() {
    const node_config_t *cfg = config_get();

    if (cfg->mqtt_host[0] == '\0') {
        ESP_LOGW(MQTT_TAG, "no MQTT broker configured; CSI will only go to serial for now.");
        return;
    }

    snprintf(s_mqtt_topic, sizeof(s_mqtt_topic), "%s/%s/data",
             cfg->mqtt_topic[0] ? cfg->mqtt_topic : "csi", cfg->node_id);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = cfg->mqtt_host;
    mqtt_cfg.broker.address.port = cfg->mqtt_port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.credentials.client_id = cfg->node_id;

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(MQTT_TAG, "failed to init MQTT client");
        return;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, &_mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    s_ringbuf = xRingbufferCreate(MQTT_RINGBUF_BYTES, RINGBUF_TYPE_NOSPLIT);
    /* Priority 5: below the WiFi (23) and lwIP tasks the publish depends on. */
    xTaskCreate(&_mqtt_publish_task, "mqtt_publish", 4096, NULL, 5, NULL);
}

bool mqtt_connected() {
    return s_mqtt_connected && s_mqtt_client != NULL;
}

/*
 * Queue a CSI record for <topic>/<node_id>/data. Safe to call from the WiFi
 * task: never blocks. QoS 0; dropped if the broker is down or the buffer is full.
 */
void mqtt_publish(const char *payload) {
    if (s_ringbuf == NULL || !mqtt_connected()) {
        return;
    }
    xRingbufferSend(s_ringbuf, payload, strlen(payload), 0);
}

#endif /* CONFIG_SEND_CSI_TO_MQTT */

#endif //WIFI_SENSING_MQTT_COMPONENT_H