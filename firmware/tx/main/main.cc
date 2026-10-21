/*
 * ============================================================================
 *  TX firmware: WiFi CSI transmitter (ESP32-C6)
 * ============================================================================
 *
 *  WHAT THIS BOARD DOES
 *  1. Joins your home WiFi as an ordinary client (a "station"). The point
 *     isn't internet access: joining puts this board on the router's channel,
 *     the same channel every RX joins.
 *  2. Broadcasts a tiny frame 100 times a second, straight over the air to
 *     every board on that channel (ESP-NOW, not through the router).
 *  3. Every RX measures how each frame was changed crossing the room (its
 *     CSI). This board never measures anything; it's the steady "light
 *     source" the receivers look at.
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
 *    WiFi driver      23   ESP-IDF   radio; actually transmits our frames
 *    sys_evt          20   ESP-IDF   event loop; calls on_wifi_event()
 *    tcpip (lwIP)     18   ESP-IDF   the TCP/IP network stack
 *    tx               5    us        hands one frame to the driver every 10 ms
 *    console          5    us        reads settings typed over USB serial
 *
 *  SETTINGS (typed over USB serial as "KEY: value", saved in flash)
 *    STASSID, STAPASS   your home WiFi (2.4 GHz)
 *    RATE               frames per second (applies immediately)
 *    SHOWCONFIG         print the current settings
 *    REBOOT             restart, applying new settings
 *
 *  Sections 1 (settings), 4 (WiFi) and 5 (console) are the same in
 *  rx/main/main.cc, so each board's firmware stays a single file.
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
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_system.h"   // esp_restart
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "tx";   // prefix for ESP_LOGx messages


/* ============================================================================
 *  1. SETTINGS
 *
 *  Defaults come from the build (Kconfig: sdkconfig.defaults or
 *  `pio run -t menuconfig`), which defines CONFIG_* macros. Anything typed over
 *  serial is saved in NVS ("non-volatile storage", a small key/value store in
 *  flash) and overrides the default at every boot.
 * ========================================================================== */

struct Settings {
    char wifi_ssid[64];
    char wifi_password[64];
    int  packet_rate;      // frames per second
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
    {"STASSID", "sta_ssid",     settings.wifi_ssid,     sizeof(settings.wifi_ssid),     nullptr},
    {"STAPASS", "sta_password", settings.wifi_password, sizeof(settings.wifi_password), nullptr},
    {"RATE",    "packet_rate",  nullptr,                0,                              &settings.packet_rate},
};

static void settings_load() {
    // 1) Build-time defaults.
    strlcpy(settings.wifi_ssid,     CONFIG_STA_SSID,     sizeof(settings.wifi_ssid));
    strlcpy(settings.wifi_password, CONFIG_STA_PASSWORD, sizeof(settings.wifi_password));
    settings.packet_rate = CONFIG_PACKET_RATE;

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
 *  2. THE TRANSMITTER
 * ========================================================================== */

// Our fixed sender address. Receivers keep only frames whose sender starts
// with 1a:00:00:00:00, so they need no per-TX setup. The last byte is the TX
// number; a second TX would use ...:02. The 0x1a first byte has the "locally
// administered" bit set, so it can't clash with a real manufacturer's address.
static const uint8_t TX_MAC[6] = {0x1a, 0x00, 0x00, 0x00, 0x00, 0x01};

// ESP-NOW's broadcast address: every board on the channel receives it.
static const uint8_t BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * The tx task: hand one frame to the WiFi driver every 1/RATE seconds.
 *
 * esp_now_send() doesn't transmit by itself. It queues the frame, and the
 * WiFi driver task (prio 23) puts it on the air moments later. The payload is
 * just a 4-byte counter; the receivers measure the frame, not its contents.
 */
static void tx_task(void *arg) {
    // ESP-NOW only sends to registered "peers"; register the broadcast address.
    esp_now_peer_info_t peer = {};   // `= {}` zero-initialises every field
    memcpy(peer.peer_addr, BROADCAST, sizeof(BROADCAST));
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;      // 0 = whatever channel we're on (the router's)
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    // By default ESP-NOW sends at 1 Mbps using old 802.11b, which has no
    // subcarriers, so receivers would get no CSI. Force 802.11n, 20 MHz wide
    // (HT20), at its slowest, most robust rate (MCS0).
    esp_now_rate_config_t rate = {};
    rate.phymode = WIFI_PHY_MODE_HT20;
    rate.rate = WIFI_PHY_RATE_MCS0_LGI;
    ESP_ERROR_CHECK(esp_now_set_peer_rate_config(BROADCAST, &rate));

    /*
     * Timing. vTaskDelay(10) would sleep 10 ms *after* each send, so the period
     * would be 10 ms plus however long the send took (that gave ~86/s).
     * vTaskDelayUntil(&last_wake, 10) instead wakes at last_wake + 10 ticks and
     * then advances last_wake by 10: the period is exactly 10 ms regardless of
     * how long the loop body takes. Measured at the RX: 100.1 frames/s.
     */
    TickType_t last_wake = xTaskGetTickCount();
    for (uint32_t counter = 0;; counter++) {   // tasks never return; they loop forever
        esp_now_send(BROADCAST, (const uint8_t *) &counter, sizeof(counter));

        // Re-read RATE every time, so a RATE: typed over serial applies at once.
        int pps = settings.packet_rate > 0 ? settings.packet_rate : 1;
        TickType_t period = pdMS_TO_TICKS(1000 / pps);
        vTaskDelayUntil(&last_wake, period > 0 ? period : 1);   // sleep; other tasks run meanwhile
    }
}


/* ============================================================================
 *  3. (The RX has its CSI section here. The TX doesn't measure anything.)
 * ========================================================================== */


/* ============================================================================
 *  4. WIFI: join the home network (same code as the RX, plus a fixed MAC)
 *
 *  Every board joins the same router, so all of them end up on its channel.
 *  That's how our broadcasts reach every RX: with one radio, a board can only
 *  hear the channel it's connected on.
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
    // TX only: replace the factory MAC with our fixed one (must happen before start).
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, TX_MAC));

    wifi_config_t cfg = {};
    strlcpy((char *) cfg.sta.ssid, settings.wifi_ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *) cfg.sta.password, settings.wifi_password, sizeof(cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());   // fires WIFI_EVENT_STA_START -> connect

    // No power saving: a radio that naps between router beacons sends late.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_now_init());   // ESP-NOW rides on the WiFi driver; init after start

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
                // RATE is read by tx_task every frame, so it's live already.
                printf(s.number == &settings.packet_rate ? "saved %s (applied now)\n" : "saved %s. REBOOT to apply\n",
                       s.command);
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
    printf("\n\nwifi-sensing TX (ESP-IDF %s)\n", IDF_VER);

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
    ESP_LOGI(TAG, "sending as " MACSTR " at %d frames/s", MAC2STR(TX_MAC), settings.packet_rate);

    // xTaskCreate(function, name, stack size in bytes, argument, priority, handle out).
    // Priority 5 keeps both below the WiFi driver (23), which does the real work.
    xTaskCreate(&tx_task, "tx", 4096, nullptr, 5, nullptr);
    xTaskCreate(&console_task, "console", 4096, nullptr, 5, nullptr);
    printf("Type KEY: value (STASSID, STAPASS, RATE), SHOWCONFIG or REBOOT.\n");
    // app_main returns here; the tasks above keep running.
}
