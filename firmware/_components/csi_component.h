#ifndef ESP32_CSI_CSI_COMPONENT_H
#define ESP32_CSI_CSI_COMPONENT_H

#include <sstream>
#include "esp_wifi.h"
#include "time_component.h"
#include "config_component.h"
#include "mqtt_component.h"

/*
 * ESP32-C6 CSI capture. Forked from the ESP32 CSI Tool (classic ESP32,
 * IDF 4.3); ported to the C6's IDF 5 CSI API.
 */

char *project_type;

/*
 * Per-packet monotonic sequence number. Boards have independent clocks, so
 * this is the per-board ordering signal; the hub's arrival timestamp is the
 * synchronization anchor. Only touched from the WiFi task (the CSI callback).
 */
static uint32_t csi_seq = 0;

void _wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    csi_seq++;

    const wifi_csi_info_t &d = *data;
    char mac[20] = {0};
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);

    /*
     * Column layout is the classic-ESP32 one, kept so the hub's CSV schema
     * doesn't change. The C6's rx_ctrl (IDF esp_wifi_he_types.h) has no
     * mcs/smoothing/not_sounding/aggregation/stbc/fec/sgi/ampdu/ant fields, so
     * those columns are 0. sig_mode carries cur_bb_format (the C6's PHY
     * format code) and bandwidth is 1 when a secondary (HT40) channel is set.
     */
    std::stringstream ss;
    ss << project_type << ","
       << mac << ","
       << d.rx_ctrl.rssi << ","
       << d.rx_ctrl.rate << ","
       << d.rx_ctrl.cur_bb_format << ","      /* sig_mode */
       << 0 << ","                            /* mcs */
       << (d.rx_ctrl.second ? 1 : 0) << ","   /* bandwidth */
       << "0,0,0,0,0,0,"                      /* smoothing,not_sounding,aggregation,stbc,fec_coding,sgi */
       << d.rx_ctrl.noise_floor << ","
       << 0 << ","                            /* ampdu_cnt */
       << d.rx_ctrl.channel << ","
       << d.rx_ctrl.second << ","
       << d.rx_ctrl.timestamp << ","
       << 0 << ","                            /* ant */
       << d.rx_ctrl.sig_len << ","
       << d.rx_ctrl.rx_state << ","
       << real_time_set << ","
       << get_steady_clock_timestamp() << ","
       << d.len << ",[";
    /* fields: role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,
       aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,
       local_timestamp,ant,sig_len,rx_state,real_time_set,real_timestamp,len,csi */

    /* Raw int8 I/Q pairs. On the C6 the first 4 bytes can be invalid (hardware
       limitation); zero them instead of dropping so subcarrier indices stay put. */
    for (int i = 0; i < d.len; i++) {
        ss << (d.first_word_invalid && i < 4 ? 0 : (int) d.buf[i]) << " ";
    }
    ss << "]";

#ifdef CONFIG_SEND_CSI_TO_SERIAL
    printf("CSI_DATA,%s\n", ss.str().c_str());
#endif

#ifdef CONFIG_SEND_CSI_TO_MQTT
    const node_config_t *cfg = config_get();
    std::string payload = std::string(cfg->node_id) + "," + std::to_string(csi_seq) + "," + ss.str();
    mqtt_publish(payload.c_str());
#endif
}

void csi_init(char *type) {
    project_type = type;

#ifdef CONFIG_SHOULD_COLLECT_CSI
    /*
     * HT20 HT-LTF only. The RX AP is pinned to 802.11b/g/n at 20 MHz (see
     * softap_init), so the TX's packets arrive as HT20 PPDUs and every record
     * is the same length. Legacy L-LTF is left off: the C6 doesn't reliably
     * deliver it (esp-idf issue #14271).
     */
    wifi_csi_config_t csi_config = {};
    csi_config.enable = 1;
    csi_config.acquire_csi_ht20 = 1;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&_wifi_csi_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

#ifdef CONFIG_SEND_CSI_TO_SERIAL
    printf("type,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,real_time_set,real_timestamp,len,CSI_DATA\n");
#endif
#endif
}

#endif //ESP32_CSI_CSI_COMPONENT_H
