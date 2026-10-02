#ifndef ESP32_CSI_INPUT_COMPONENT_H
#define ESP32_CSI_INPUT_COMPONENT_H

#include <strings.h>
#include <string.h>
#include "esp_system.h"
#include "time_component.h"
#include "config_component.h"

char input_buffer[256];
int input_buffer_pointer = 0;

static void _config_command_done(const char *what) {
    printf("updated %s. reboot to apply\n", what);
}

void _handle_input() {
    char *line = input_buffer;
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (line[0] == '\0') {
        return;
    }

    if (match_set_timestamp_template(line)) {
        printf("Setting local time to %s\n", line);
        time_set(line);
        return;
    }

    if (strcasecmp(line, "SHOWCONFIG") == 0) {
        config_print();
        return;
    }
    if (strcasecmp(line, "REBOOT") == 0) {
        printf("rebooting...\n");
        vTaskDelay(200 / portTICK_PERIOD_MS);
        esp_restart();
        return;
    }

    char *colon = strchr(line, ':');
    char *raw_value = NULL;
    if (colon != NULL) {
        *colon = '\0';
        raw_value = colon + 1;
        while (*raw_value == ' ' || *raw_value == '\t') {
            raw_value++;
        }
    }

    if (raw_value == NULL || raw_value[0] == '\0') {
        if (colon != NULL) {
            *colon = ':'; /* put back for nice error message */
        }
        printf("Unable to handle input %s\n", line);
        return;
    }

    char *key = line;
    bool ok = false;

    if (strcasecmp(key, "NODEID") == 0) {
        ok = config_set_node_id(raw_value);
    } else if (strcasecmp(key, "MQTTHOST") == 0) {
        ok = config_set_mqtt_host(raw_value);
    } else if (strcasecmp(key, "MQTTPORT") == 0) {
        ok = config_set_mqtt_port(atoi(raw_value));
    } else if (strcasecmp(key, "MQTTTOPIC") == 0) {
        ok = config_set_mqtt_topic(raw_value);
    } else if (strcasecmp(key, "APSSID") == 0) {
        ok = config_set_ap_ssid(raw_value);
    } else if (strcasecmp(key, "APPASS") == 0) {
        ok = config_set_ap_password(raw_value);
    } else if (strcasecmp(key, "APCH") == 0) {
        ok = config_set_ap_channel(atoi(raw_value));
    } else if (strcasecmp(key, "STASSID") == 0) {
        ok = config_set_sta_ssid(raw_value);
    } else if (strcasecmp(key, "STAPASS") == 0) {
        ok = config_set_sta_password(raw_value);
    } else if (strcasecmp(key, "STACH") == 0) {
        ok = config_set_sta_channel(atoi(raw_value));
    } else if (strcasecmp(key, "RATE") == 0) {
        ok = config_set_packet_rate(atoi(raw_value));
    } else {
        printf("Unknown command: %s\n", key);
        return;
    }

    if (ok) {
        _config_command_done(key);
        config_print();
    } else {
        printf("Failed to store %s in NVS\n", key);
    }
}

void input_check() {
    uint8_t ch = fgetc(stdin);

    while (ch != 0xFF) {
        if (ch == '\n') {
            _handle_input();
            input_buffer[0] = '\0';
            input_buffer_pointer = 0;
        } else {
            if (input_buffer_pointer < (int) sizeof(input_buffer) - 2) {
                input_buffer[input_buffer_pointer] = ch;
                input_buffer[input_buffer_pointer + 1] = '\0';
                input_buffer_pointer++;
            }
        }

        ch = fgetc(stdin);
    }
}

void input_loop(void *ignored) {
    (void) ignored;
    while (true) {
        input_check();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void input_loop_init() {
    xTaskCreate(&input_loop, "input_loop", 4096, NULL, 10, NULL);
}

#endif //ESP32_CSI_INPUT_COMPONENT_H