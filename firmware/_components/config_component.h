#ifndef WIFI_SENSING_CONFIG_COMPONENT_H
#define WIFI_SENSING_CONFIG_COMPONENT_H

#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define CONFIG_NVS_NAMESPACE "wifi_sensing"
#define CONFIG_MAX_STR 64

/*
 * Baked-in defaults come from each project's Kconfig menu (menuconfig),
 * and can be overridden at runtime per board over the serial console.
 * Overrides are persisted in NVS, so identical firmware on every board +
 * a few serial commands = a new configured node. No code changes needed.
 */

#ifdef CONFIG_NODE_ID
#define DEFAULT_NODE_ID CONFIG_NODE_ID
#else
#define DEFAULT_NODE_ID "node"
#endif

#ifdef CONFIG_MQTT_HOST
#define DEFAULT_MQTT_HOST CONFIG_MQTT_HOST
#else
#define DEFAULT_MQTT_HOST ""
#endif

#ifdef CONFIG_MQTT_PORT
#define DEFAULT_MQTT_PORT CONFIG_MQTT_PORT
#else
#define DEFAULT_MQTT_PORT 1883
#endif

#ifdef CONFIG_MQTT_TOPIC
#define DEFAULT_MQTT_TOPIC CONFIG_MQTT_TOPIC
#else
#define DEFAULT_MQTT_TOPIC "csi"
#endif

#ifdef CONFIG_AP_SSID
#define DEFAULT_AP_SSID CONFIG_AP_SSID
#else
#define DEFAULT_AP_SSID "wifi-sense-rx"
#endif

#ifdef CONFIG_AP_PASSWORD
#define DEFAULT_AP_PASSWORD CONFIG_AP_PASSWORD
#else
#define DEFAULT_AP_PASSWORD ""
#endif

#ifdef CONFIG_AP_CHANNEL
#define DEFAULT_AP_CHANNEL CONFIG_AP_CHANNEL
#else
#define DEFAULT_AP_CHANNEL 6
#endif

#ifdef CONFIG_STA_SSID
#define DEFAULT_STA_SSID CONFIG_STA_SSID
#else
#define DEFAULT_STA_SSID ""
#endif

#ifdef CONFIG_STA_PASSWORD
#define DEFAULT_STA_PASSWORD CONFIG_STA_PASSWORD
#else
#define DEFAULT_STA_PASSWORD ""
#endif

#ifdef CONFIG_STA_CHANNEL
#define DEFAULT_STA_CHANNEL CONFIG_STA_CHANNEL
#else
#define DEFAULT_STA_CHANNEL 0
#endif

#ifdef CONFIG_PACKET_RATE
#define DEFAULT_PACKET_RATE CONFIG_PACKET_RATE
#else
#define DEFAULT_PACKET_RATE 100
#endif

typedef struct {
    /* reporting network (MQTT) */
    char node_id[CONFIG_MAX_STR];
    char mqtt_host[CONFIG_MAX_STR];
    int mqtt_port;
    char mqtt_topic[CONFIG_MAX_STR];

    /* sensing channel (only used by the receiver/AP) */
    char ap_ssid[CONFIG_MAX_STR];
    char ap_password[CONFIG_MAX_STR];
    int ap_channel;

    /* sensing channel (only used by the transmitter/station) */
    char sta_ssid[CONFIG_MAX_STR];
    char sta_password[CONFIG_MAX_STR];
    int sta_channel;

    /* transmitter packet rate (packets per second) */
    int packet_rate;
} node_config_t;

static node_config_t s_config;

static void _config_apply_defaults(node_config_t *cfg) {
    strlcpy(cfg->node_id, DEFAULT_NODE_ID, sizeof(cfg->node_id));
    strlcpy(cfg->mqtt_host, DEFAULT_MQTT_HOST, sizeof(cfg->mqtt_host));
    cfg->mqtt_port = DEFAULT_MQTT_PORT;
    strlcpy(cfg->mqtt_topic, DEFAULT_MQTT_TOPIC, sizeof(cfg->mqtt_topic));
    strlcpy(cfg->ap_ssid, DEFAULT_AP_SSID, sizeof(cfg->ap_ssid));
    strlcpy(cfg->ap_password, DEFAULT_AP_PASSWORD, sizeof(cfg->ap_password));
    cfg->ap_channel = DEFAULT_AP_CHANNEL;
    strlcpy(cfg->sta_ssid, DEFAULT_STA_SSID, sizeof(cfg->sta_ssid));
    strlcpy(cfg->sta_password, DEFAULT_STA_PASSWORD, sizeof(cfg->sta_password));
    cfg->sta_channel = DEFAULT_STA_CHANNEL;
    cfg->packet_rate = DEFAULT_PACKET_RATE;
}

static void _config_load_str(nvs_handle_t handle, const char *key, char *dest, size_t dest_len, const char *fallback) {
    size_t len = dest_len;
    if (nvs_get_str(handle, key, dest, &len) != ESP_OK) {
        strlcpy(dest, fallback, dest_len);
    }
}

static void _config_load_int(nvs_handle_t handle, const char *key, int *dest, int fallback) {
    int32_t v; /* int32_t is long on RISC-V (C6), so it can't alias int* */
    *dest = nvs_get_i32(handle, key, &v) == ESP_OK ? (int) v : fallback;
}

static bool _config_commit_str(const char *key, const char *value, char *dest, size_t dest_len) {
    strlcpy(dest, value, dest_len);
    nvs_handle_t handle;
    if (nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_str(handle, key, dest);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_commit(handle);
    nvs_close(handle);
    return true;
}

static bool _config_commit_int(const char *key, int value, int *dest) {
    *dest = value;
    nvs_handle_t handle;
    if (nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_i32(handle, key, value);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_commit(handle);
    nvs_close(handle);
    return true;
}

/* Must be called after nvs_init(). */
void config_init() {
    _config_apply_defaults(&s_config);

    nvs_handle_t handle;
    if (nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    _config_load_str(handle, "node_id", s_config.node_id, sizeof(s_config.node_id), s_config.node_id);
    _config_load_str(handle, "mqtt_host", s_config.mqtt_host, sizeof(s_config.mqtt_host), s_config.mqtt_host);
    _config_load_int(handle, "mqtt_port", &s_config.mqtt_port, s_config.mqtt_port);
    _config_load_str(handle, "mqtt_topic", s_config.mqtt_topic, sizeof(s_config.mqtt_topic), s_config.mqtt_topic);
    _config_load_str(handle, "ap_ssid", s_config.ap_ssid, sizeof(s_config.ap_ssid), s_config.ap_ssid);
    _config_load_str(handle, "ap_password", s_config.ap_password, sizeof(s_config.ap_password), s_config.ap_password);
    _config_load_int(handle, "ap_channel", &s_config.ap_channel, s_config.ap_channel);
    _config_load_str(handle, "sta_ssid", s_config.sta_ssid, sizeof(s_config.sta_ssid), s_config.sta_ssid);
    _config_load_str(handle, "sta_password", s_config.sta_password, sizeof(s_config.sta_password), s_config.sta_password);
    _config_load_int(handle, "sta_channel", &s_config.sta_channel, s_config.sta_channel);
    _config_load_int(handle, "packet_rate", &s_config.packet_rate, s_config.packet_rate);
    nvs_close(handle);
}

const node_config_t *config_get() {
    return &s_config;
}

void config_print() {
    const node_config_t *c = config_get();
    printf("--- node config ---\n");
    printf("node_id:   %s\n", c->node_id);
    printf("mqtt_host: %s\n", c->mqtt_host);
    printf("mqtt_port: %d\n", c->mqtt_port);
    printf("mqtt_topic:%s\n", c->mqtt_topic);
    printf("ap_ssid:   %s\n", c->ap_ssid);
    printf("ap_pass:   %s\n", c->ap_password);
    printf("ap_channel:%d\n", c->ap_channel);
    printf("sta_ssid:  %s\n", c->sta_ssid);
    printf("sta_pass:  %s\n", c->sta_password);
    printf("sta_channel:%d\n", c->sta_channel);
    printf("packet_rate:%d\n", c->packet_rate);
    printf("------------------\n");
}

static bool config_set_node_id(const char *value) {
    return _config_commit_str("node_id", value, s_config.node_id, sizeof(s_config.node_id));
}
static bool config_set_mqtt_host(const char *value) {
    return _config_commit_str("mqtt_host", value, s_config.mqtt_host, sizeof(s_config.mqtt_host));
}
static bool config_set_mqtt_port(int value) {
    return _config_commit_int("mqtt_port", value, &s_config.mqtt_port);
}
static bool config_set_mqtt_topic(const char *value) {
    return _config_commit_str("mqtt_topic", value, s_config.mqtt_topic, sizeof(s_config.mqtt_topic));
}
static bool config_set_ap_ssid(const char *value) {
    return _config_commit_str("ap_ssid", value, s_config.ap_ssid, sizeof(s_config.ap_ssid));
}
static bool config_set_ap_password(const char *value) {
    return _config_commit_str("ap_password", value, s_config.ap_password, sizeof(s_config.ap_password));
}
static bool config_set_ap_channel(int value) {
    return _config_commit_int("ap_channel", value, &s_config.ap_channel);
}
static bool config_set_sta_ssid(const char *value) {
    return _config_commit_str("sta_ssid", value, s_config.sta_ssid, sizeof(s_config.sta_ssid));
}
static bool config_set_sta_password(const char *value) {
    return _config_commit_str("sta_password", value, s_config.sta_password, sizeof(s_config.sta_password));
}
static bool config_set_sta_channel(int value) {
    return _config_commit_int("sta_channel", value, &s_config.sta_channel);
}
static bool config_set_packet_rate(int value) {
    if (value < 0) {
        value = 0;
    }
    return _config_commit_int("packet_rate", value, &s_config.packet_rate);
}

#endif //WIFI_SENSING_CONFIG_COMPONENT_H