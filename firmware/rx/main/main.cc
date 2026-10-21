/*
 * ============================================================================
 *  RX firmware: WiFi CSI receiver (ESP32-C6)
 * ============================================================================
 *
 *  WHAT THIS BOARD DOES
 *  1. Joins your home WiFi as an ordinary client (a "station").
 *  2. Listens to every frame on that WiFi channel ("promiscuous mode").
 *  3. For each frame sent by our TX board, the radio hardware reports its
 *     Channel State Information (CSI): how the signal's strength and phase
 *     were changed on each of 64 narrow sub-frequencies ("subcarriers") on
 *     its way across the room. A person moving changes this pattern.
 *  4. Each CSI report becomes one line of CSV text, which we
 *       - print on the USB serial port (handy for debugging), and
 *       - publish over MQTT to the laptop (the hub), which saves it.
 *
 *  ESP-IDF AND FREERTOS IN 60 SECONDS
 *  - There is no main(). ESP-IDF boots FreeRTOS, then calls app_main() from a
 *    task called "main". When app_main() returns, that task ends, but every
 *    task it started keeps running. That's normal.
 *  - A FreeRTOS *task* is a thread: its own stack, its own priority. Higher
 *    number = more urgent. The scheduler always runs the highest-priority
 *    task that is ready; equal priorities take turns. The C6 has one main
 *    core, so tasks interleave rather than truly run at the same time.
 *  - A task with nothing to do must *block*: sleep (vTaskDelay) or wait on a
 *    queue. Blocking hands the CPU to lower-priority tasks. A task that spins
 *    without blocking starves every task below it.
 *  - Time is counted in *ticks*. sdkconfig sets CONFIG_FREERTOS_HZ=1000, so
 *    1 tick = 1 ms. pdMS_TO_TICKS(ms) converts milliseconds to ticks.
 *  - ESP_ERROR_CHECK(x): if x isn't ESP_OK, print the error and reboot. Used
 *    for setup steps that must never fail.
 *
 *  WHO RUNS WHAT ON THIS BOARD
 *    task            prio  owner     job
 *    WiFi driver      23   ESP-IDF   radio; calls csi_callback() per frame
 *    sys_evt          20   ESP-IDF   event loop; calls on_wifi_event()
 *    tcpip (lwIP)     18   ESP-IDF   the TCP/IP network stack
 *    mqtt_task         5   esp-mqtt  MQTT connection; calls on_mqtt_event()
 *    publish           5   us        sends queued records to the broker
 *    console           5   us        reads settings typed over USB serial
 *
 *  ONE FRAME'S JOURNEY
 *    radio ─▶ csi_callback() ─▶ ring buffer ─▶ publish_task() ─▶ MQTT ─▶ hub
 *             (WiFi task)                       (our task)
 *
 *  SETTINGS (typed over USB serial as "KEY: value", saved in flash)
 *    STASSID, STAPASS               your home WiFi (2.4 GHz)
 *    NODEID                         this receiver's name, unique per board
 *    MQTTHOST, MQTTPORT, MQTTTOPIC  where the hub's MQTT broker is
 *    SHOWCONFIG                     print the current settings
 *    REBOOT                         restart, applying new settings
 *
 *  Sections 1 (settings), 4 (WiFi) and 5 (console) are the same in
 *  tx/main/main.cc, so each board's firmware stays a single file.
 *
 *  Parts are derived from the ESP32 CSI Tool by Steven M. Hernandez (MIT);
 *  see THIRD_PARTY.md.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // strcasecmp

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_system.h"   // esp_restart
#include "esp_timer.h"    // esp_timer_get_time
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "rx";   // prefix for ESP_LOGx messages


/* ============================================================================
 *  1. SETTINGS
 *
 *  Defaults come from the build (Kconfig: sdkconfig.defaults or
 *  `pio run -t menuconfig`), which defines CONFIG_* macros. Anything typed over
 *  serial is saved in NVS ("non-volatile storage", a small key/value store in
 *  flash) and overrides the default at every boot. That's what lets every RX
 *  run the identical firmware: only its settings differ.
 * ========================================================================== */

struct Settings {
    char wifi_ssid[64];
    char wifi_password[64];
    char node_id[64];
    char mqtt_host[64];
    int  mqtt_port;
    char mqtt_topic[64];
};
static Settings settings;   // `static` at file scope = private to this file

static const char *NVS_NAMESPACE = "wifi_sensing";

// One row per setting: the serial command, its NVS key, and where it lives in
// `settings` (a string with its size, or an int). The console and the
// load/print code all loop over this table, so adding a setting is one line.
struct SettingDef {
    const char *command;
    const char *nvs_key;
    char *text;      // string setting, or nullptr
    size_t size;
    int *number;     // int setting, or nullptr
};
static const SettingDef SETTINGS[] = {
    {"STASSID",   "sta_ssid",     settings.wifi_ssid,     sizeof(settings.wifi_ssid),     nullptr},
    {"STAPASS",   "sta_password", settings.wifi_password, sizeof(settings.wifi_password), nullptr},
    {"NODEID",    "node_id",      settings.node_id,       sizeof(settings.node_id),       nullptr},
    {"MQTTHOST",  "mqtt_host",    settings.mqtt_host,     sizeof(settings.mqtt_host),     nullptr},
    {"MQTTPORT",  "mqtt_port",    nullptr,                0,                              &settings.mqtt_port},
    {"MQTTTOPIC", "mqtt_topic",   settings.mqtt_topic,    sizeof(settings.mqtt_topic),    nullptr},
};

static void settings_load() {
    // 1) Build-time defaults.
    strlcpy(settings.wifi_ssid,     CONFIG_STA_SSID,     sizeof(settings.wifi_ssid));
    strlcpy(settings.wifi_password, CONFIG_STA_PASSWORD, sizeof(settings.wifi_password));
    strlcpy(settings.node_id,       CONFIG_NODE_ID,      sizeof(settings.node_id));
    strlcpy(settings.mqtt_host,     CONFIG_MQTT_HOST,    sizeof(settings.mqtt_host));
    settings.mqtt_port = CONFIG_MQTT_PORT;
    strlcpy(settings.mqtt_topic,    CONFIG_MQTT_TOPIC,   sizeof(settings.mqtt_topic));

    // 2) Override with whatever was saved over serial. If nothing was ever
    //    saved, the namespace doesn't exist yet and we keep the defaults.
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    for (const SettingDef &s : SETTINGS) {
        if (s.text) {
            char value[64];
            size_t len = sizeof(value);
            if (nvs_get_str(nvs, s.nvs_key, value, &len) == ESP_OK) {
                strlcpy(s.text, value, s.size);
            }
        } else {
            // NVS stores int32_t, which on this RISC-V chip is `long`, not
            // `int`, so read into an int32_t and convert.
            int32_t value;
            if (nvs_get_i32(nvs, s.nvs_key, &value) == ESP_OK) {
                *s.number = (int) value;
            }
        }
    }
    nvs_close(nvs);
}

// Store one setting in RAM and flash. Returns false if flash write failed.
static bool settings_save(const SettingDef &s, const char *value) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    esp_err_t err;
    if (s.text) {
        strlcpy(s.text, value, s.size);
        err = nvs_set_str(nvs, s.nvs_key, s.text);
    } else {
        *s.number = atoi(value);
        err = nvs_set_i32(nvs, s.nvs_key, *s.number);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);   // nvs_set_* only stages the write; commit does it
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

static void settings_print() {
    printf("--- settings ---\n");
    for (const SettingDef &s : SETTINGS) {
        if (s.number) {
            printf("%-10s %d\n", s.command, *s.number);
        } else if (s.text == settings.wifi_password) {
            // Never echo the password: serial logs get copied and pasted.
            printf("%-10s %s\n", s.command, s.text[0] ? "(set)" : "(not set)");
        } else {
            printf("%-10s %s\n", s.command, s.text);
        }
    }
    printf("----------------\n");
}


/* ============================================================================
 *  2. MQTT: getting records to the hub without ever stalling the radio
 *
 *  The CSI callback (section 3) runs inside the WiFi driver's task. If it
 *  waited on the network, it would be waiting on WiFi, which can't run until
 *  the callback returns: the radio stalls. So the callback only drops each
 *  record into a ring buffer and returns at once. Our own publish task, at
 *  low priority, takes records out and does the slow, blocking send.
 *
 *  A FreeRTOS ring buffer is a thread-safe queue of variable-length items. If
 *  it's full (broker down, WiFi busy) new records are dropped. Memory stays
 *  bounded, and a gap in `seq` at the hub shows what was lost.
 * ========================================================================== */

static esp_mqtt_client_handle_t mqtt = nullptr;
static char mqtt_topic[160];          // "<MQTTTOPIC>/<NODEID>/data", e.g. csi/rx-1/data
static RingbufHandle_t outbox = nullptr;
static const size_t OUTBOX_BYTES = 32 * 1024;   // ~50 records, ~0.5 s at 100/s

// Written by the MQTT task, read by the WiFi and publish tasks. `volatile`
// makes the compiler re-read it every time instead of caching it; a bool
// write is a single instruction here, so no lock is needed.
static volatile bool mqtt_connected = false;

// esp-mqtt runs its own task (connect, keep-alive, reconnect) and tells us
// what happened by calling this from that task.
static void on_mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == MQTT_EVENT_CONNECTED) {
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected; publishing to %s", mqtt_topic);
    } else if (id == MQTT_EVENT_DISCONNECTED) {
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected; esp-mqtt will retry");
    }
}

// Called from the CSI callback. Must never wait: the 0 timeout means "if the
// buffer is full, give up immediately" (the record is dropped).
static void queue_record(const char *record, size_t len) {
    if (outbox != nullptr && mqtt_connected) {
        xRingbufferSend(outbox, record, len, 0);
    }
}

// Our publish task. Blocking here is fine: while it waits, FreeRTOS runs
// other tasks, and the WiFi task (prio 23) can always interrupt it.
static void publish_task(void *arg) {
    for (;;) {   // tasks never return; they loop forever
        size_t len;
        // Sleep until a record is available (portMAX_DELAY = wait forever).
        char *record = (char *) xRingbufferReceive(outbox, &len, portMAX_DELAY);
        if (mqtt_connected) {
            // QoS 0: send once, no acknowledgement. Fine on a local network.
            esp_mqtt_client_publish(mqtt, mqtt_topic, record, len, 0, 0);
        }
        vRingbufferReturnItem(outbox, record);   // free its space in the buffer
    }
}

static void mqtt_start() {
    if (settings.mqtt_host[0] == '\0') {
        ESP_LOGW(TAG, "no MQTTHOST set; CSI goes to serial only");
        return;
    }
    snprintf(mqtt_topic, sizeof(mqtt_topic), "%s/%s/data", settings.mqtt_topic, settings.node_id);

    esp_mqtt_client_config_t cfg = {};   // `= {}` zero-initialises every field
    cfg.broker.address.hostname = settings.mqtt_host;
    cfg.broker.address.port = settings.mqtt_port;
    cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    cfg.credentials.client_id = settings.node_id;   // the broker needs unique ids
    mqtt = esp_mqtt_client_init(&cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt, MQTT_EVENT_ANY, &on_mqtt_event, nullptr));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt));   // starts mqtt_task; returns at once

    outbox = xRingbufferCreate(OUTBOX_BYTES, RINGBUF_TYPE_NOSPLIT);   // NOSPLIT: each record stays contiguous
    // xTaskCreate(function, name, stack size in bytes, argument, priority, handle out)
    xTaskCreate(&publish_task, "publish", 4096, nullptr, 5, nullptr);
}


/* ============================================================================
 *  3. CSI: one record per frame from the TX
 * ========================================================================== */

// Every TX broadcasts from 1a:00:00:00:00:0N (it sets this MAC itself; see
// tx/main/main.cc). We keep frames whose sender matches the first 5 bytes, so
// any of our TXs counts and an RX needs no TX-specific setup.
static const uint8_t TX_MAC_PREFIX[5] = {0x1a, 0x00, 0x00, 0x00, 0x00};

// Per-board record counter, written to the `seq` column. Only csi_callback
// touches it, and that always runs in the same task, so no lock is needed.
static uint32_t seq = 0;

/*
 * Called by the WiFi driver task (prio 23) for every received frame it has
 * CSI for: possibly hundreds per second from all devices on the channel.
 * Rules for code here: be quick, never block, never wait on a lock.
 *
 * Record layout (comma separated), kept identical to the original CSI tool so
 * the hub's CSV header never changes:
 *   node_id,seq,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,
 *   not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,
 *   channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,
 *   real_time_set,real_timestamp,len,[csi]
 * The C6's radio doesn't report mcs, smoothing, not_sounding, aggregation,
 * stbc, fec_coding, sgi, ampdu_cnt or ant, so those are always 0, as is
 * real_time_set (no clock sync; the hub's arrival time is the timeline).
 */
static void csi_callback(void *ctx, wifi_csi_info_t *info) {
    // We hear the whole channel, so drop everything that isn't our TX first.
    if (memcmp(info->mac, TX_MAC_PREFIX, sizeof(TX_MAC_PREFIX)) != 0) {
        return;
    }
    seq++;

    // `static` buffers live in RAM permanently instead of on the WiFi task's
    // small stack. Safe only because this function never runs twice at once.
    static char body[1024];     // everything from `role` onwards
    static char record[1100];   // node_id,seq,body

    const wifi_pkt_rx_ctrl_t &rx = info->rx_ctrl;   // radio metadata for this frame
    const uint8_t *m = info->mac;
    int n = snprintf(body, sizeof(body),
        "RX,%02X:%02X:%02X:%02X:%02X:%02X,"   // role, sender MAC
        "%d,%d,%d,0,%d,"                      // rssi (dBm), rate, sig_mode, mcs, bandwidth
        "0,0,0,0,0,0,"                        // smoothing .. sgi
        "%d,0,%d,%d,"                         // noise_floor (dBm), ampdu_cnt, channel, secondary_channel
        "%u,0,%d,%d,"                         // local_timestamp (µs), ant, sig_len, rx_state
        "0,%.6f,%d,[",                        // real_time_set, real_timestamp (s since boot), len
        m[0], m[1], m[2], m[3], m[4], m[5],
        (int) rx.rssi, (int) rx.rate, (int) rx.cur_bb_format, rx.second ? 1 : 0,
        (int) rx.noise_floor, (int) rx.channel, (int) rx.second,
        (unsigned) rx.timestamp, (int) rx.sig_len, (int) rx.rx_state,
        esp_timer_get_time() / 1e6, (int) info->len);

    // The CSI itself: 128 signed bytes = 64 subcarriers x (imaginary, real).
    // The C6 can flag its first 4 bytes as garbage (a hardware quirk); write
    // zeros rather than dropping them, so subcarrier positions never shift.
    for (int i = 0; i < info->len && n < (int) sizeof(body) - 8; i++) {
        int v = (info->first_word_invalid && i < 4) ? 0 : info->buf[i];
        n += snprintf(body + n, sizeof(body) - n, "%d ", v);
    }
    snprintf(body + n, sizeof(body) - n, "]");

#ifdef CONFIG_SEND_CSI_TO_SERIAL
    // ~600 bytes x 100/s uses about 2/3 of the 921600-baud serial link.
    printf("CSI_DATA,%s\n", body);
#endif

    int len = snprintf(record, sizeof(record), "%s,%lu,%s", settings.node_id, (unsigned long) seq, body);
    queue_record(record, len);
}

static void csi_start() {
    // Promiscuous mode: receive every frame on the channel, not just frames
    // for this board. The TX's broadcasts aren't part of our WiFi network, so
    // without this we'd never see them.
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    // Which frames produce CSI. Only 802.11n 20 MHz ("HT20") frames, which is
    // what the TX sends, so every record is 128 values. The router's 40 MHz
    // and WiFi 6 frames produce none. (The C6 also can't reliably report the
    // older "legacy" format: esp-idf issue #14271.)
    wifi_csi_config_t cfg = {};
    cfg.enable = 1;
    cfg.acquire_csi_ht20 = 1;
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&csi_callback, nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

#ifdef CONFIG_SEND_CSI_TO_SERIAL
    printf("type,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,"
           "fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,"
           "sig_len,rx_state,real_time_set,real_timestamp,len,CSI_DATA\n");
#endif
}


/* ============================================================================
 *  4. WIFI: join the home network (same code as the TX)
 *
 *  Every board joins the same router, so all of them end up on its channel.
 *  That's how the TX's broadcasts reach every RX: with one radio, a board can
 *  only hear the channel it's connected on.
 *
 *  ESP-IDF reports WiFi happenings as *events* (started, connected, got an IP
 *  address, lost connection). They're delivered to on_wifi_event() by the
 *  default event loop, which runs in its own task (sys_evt).
 * ========================================================================== */

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        // Radio is up. Start connecting (the result arrives as another event).
        if (settings.wifi_ssid[0] != '\0') {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        // Wrong password, router out of range or rebooting: just keep trying.
        ESP_LOGW(TAG, "not connected to \"%s\"; retrying", settings.wifi_ssid);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // Fully online. Report the channel: every board should show the same one.
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) data;
        uint8_t channel;
        wifi_second_chan_t second;
        esp_wifi_get_channel(&channel, &second);
        ESP_LOGI(TAG, "joined \"%s\": ip " IPSTR ", channel %d",
                 settings.wifi_ssid, IP2STR(&event->ip_info.ip), channel);
    }
}

static void wifi_start() {
    ESP_ERROR_CHECK(esp_netif_init());                  // TCP/IP stack
    ESP_ERROR_CHECK(esp_event_loop_create_default());   // the sys_evt task
    esp_netif_create_default_wifi_sta();                // network interface for station mode

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));              // starts the WiFi driver task
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, nullptr, nullptr));

    // Credentials live in our NVS settings; don't let the driver keep a second copy.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t cfg = {};
    strlcpy((char *) cfg.sta.ssid, settings.wifi_ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *) cfg.sta.password, settings.wifi_password, sizeof(cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());   // fires WIFI_EVENT_STA_START -> connect

    // No power saving: a radio that naps between router beacons misses frames.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // ESP-NOW is how the TX sends; initialising it here makes the driver
    // accept those frames instead of discarding them.
    ESP_ERROR_CHECK(esp_now_init());

    if (settings.wifi_ssid[0] == '\0') {
        ESP_LOGW(TAG, "no WiFi set. Type STASSID: <network> and STAPASS: <password>, then REBOOT");
    }
}


/* ============================================================================
 *  5. SERIAL CONSOLE: type "KEY: value", SHOWCONFIG or REBOOT
 *
 *  stdin is the USB serial port. Reading it doesn't block: when nothing has
 *  been typed, fgetc() returns EOF straight away. So the task polls: read
 *  whatever is there, then sleep 20 ms so lower-priority work can run.
 * ========================================================================== */

static void handle_line(char *line) {
    while (*line == ' ') line++;

    if (strcasecmp(line, "SHOWCONFIG") == 0) {
        settings_print();
        return;
    }
    if (strcasecmp(line, "REBOOT") == 0) {
        printf("rebooting...\n");
        vTaskDelay(pdMS_TO_TICKS(200));   // let the message finish printing
        esp_restart();
    }

    // Split "KEY: value" at the colon.
    char *colon = strchr(line, ':');
    if (colon == nullptr) {
        printf("Unknown command \"%s\". Use KEY: value, SHOWCONFIG or REBOOT\n", line);
        return;
    }
    *colon = '\0';
    char *value = colon + 1;
    while (*value == ' ') value++;

    for (const SettingDef &s : SETTINGS) {
        if (strcasecmp(line, s.command) == 0) {
            if (settings_save(s, value)) {
                printf("saved %s. REBOOT to apply\n", s.command);
            } else {
                printf("failed to save %s\n", s.command);
            }
            return;
        }
    }
    printf("Unknown setting \"%s\"\n", line);
}

static void console_task(void *arg) {
    char line[128];
    size_t n = 0;
    for (;;) {
        int c;
        while ((c = fgetc(stdin)) != EOF) {
            if (c == '\n' || c == '\r') {   // terminals send \r, \n or both
                line[n] = '\0';
                if (n > 0) handle_line(line);
                n = 0;
            } else if (n < sizeof(line) - 1) {
                line[n++] = (char) c;
            }
        }
        clearerr(stdin);                  // reset the EOF flag so the next read tries again
        vTaskDelay(pdMS_TO_TICKS(20));    // nothing typed: sleep, let others run
    }
}


/* ============================================================================
 *  6. ENTRY POINT
 *
 *  extern "C": ESP-IDF's startup code is C and looks for a function named
 *  exactly `app_main`. C++ normally "mangles" names to encode argument types,
 *  which would hide it; extern "C" keeps the plain name.
 * ========================================================================== */

extern "C" void app_main() {
    printf("\n\nwifi-sensing RX (ESP-IDF %s)\n", IDF_VER);

    // NVS holds our settings and the radio's calibration data. If the flash
    // area is full or from an older ESP-IDF, wipe it and start fresh.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    settings_load();
    settings_print();

    wifi_start();   // join the home WiFi (continues in the background)
    mqtt_start();   // MQTT client + publish task; before csi_start so the outbox exists
    csi_start();    // from here on, csi_callback() runs for every TX frame

    xTaskCreate(&console_task, "console", 4096, nullptr, 5, nullptr);
    printf("Type KEY: value (STASSID, STAPASS, NODEID, MQTTHOST, MQTTPORT, MQTTTOPIC), SHOWCONFIG or REBOOT.\n");
    // app_main returns here; the tasks above keep running.
}
