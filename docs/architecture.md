# Architecture

## Topology: one TX broadcasts, any number of RXs listen

CSI is a measurement a WiFi radio makes on the **frames it receives**, so the
design is one known transmitter and as many listeners as you like:

- **TX** (`firmware/tx`) joins the home WiFi, takes the fixed MAC
  `1a:00:00:00:00:01`, and broadcasts a 4-byte ESP-NOW frame (a counter)
  every `1000 / RATE` ms (default 100/s, changeable live with `RATE:`). The
  frames go straight over the air, not through the router, as 802.11n HT20
  at MCS0. ESP-NOW's default 1 Mbps 802.11b format has no OFDM subcarriers
  and would yield no CSI.
- **RX** (`firmware/rx`) joins the same home WiFi and listens to the whole
  channel (promiscuous mode). The CSI callback drops every frame whose sender
  doesn't start with `1a:00:00:00:00`, so each RX keeps exactly the TX's
  frames with no per-board TX setup. Each kept frame becomes one record,
  published over MQTT to the hub on the home network.

Because every board joins the same router, they all sit on its channel
automatically, and follow it if the router changes channel.

## Data flow

```
TX ESP32                 RX ESP32 (×N)                         laptop (home WiFi)
--------                 -------------                         ------------------
ESP-NOW broadcast   ─▶   CSI callback (TX frames only)
HT20 MCS0, 100/s         seq++, format CSV record
from 1a:..:00:01         ─▶ 32 KB ring buffer ─▶ publisher ─▶  mosquitto :1883
                                         topic csi/<node>/data      │
                                                                     ▼
                                                    csi_hub.py subscribes csi/+/data,
                                                    stamps arrival_epoch,
                                                    appends data/<node>.csv, live plot
```

MQTT topic: `<mqtt_topic>/<node_id>/data`. Topics are per-node, so the hub's
single subscription `csi/+/data` scales to any number of receivers.

## Record format (one CSV row per frame)

```
arrival_epoch,node_id,seq,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,
not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,
secondary_channel,local_timestamp,ant,sig_len,rx_state,real_time_set,
real_timestamp,len,csi
```

- `arrival_epoch`: hub wall clock (seconds). The shared timeline.
- `node_id`: from per-board NVS config.
- `seq`: per-board counter, +1 per captured frame. Gaps mean drops.
- `role`: `RX`. `mac`: the sender, i.e. the TX (`1A:00:00:00:00:01`).
- `local_timestamp`: RX radio timestamp in µs (`rx_ctrl.timestamp`). Gaps of
  10 ms at RATE 100.
- `real_timestamp`: RX steady clock, seconds since boot.
- `csi`: raw complex CSI, `[I0 Q0 I1 Q1 ...]` int8 pairs: 128 values of HT20
  HT-LTF (64 subcarrier slots). Only HT20 frames produce CSI, so every record
  has this shape; the router's HT40 / WiFi 6 frames are ignored. The C6's
  subcarrier order differs from the classic ESP32's, and L-LTF isn't captured
  (esp-idf issue #14271). Slots 0–3, 32 (DC) and 61–63 are always 0.

ESP32-C6 notes: the C6's `rx_ctrl` has no `mcs`, `smoothing`, `not_sounding`,
`aggregation`, `stbc`, `fec_coding`, `sgi`, `ampdu_cnt`, or `ant` fields, so
those columns are always 0. The columns stay so the schema matches the
upstream CSI tool. `sig_mode` carries the C6's `cur_bb_format` code (2 = HT)
and `bandwidth` is 1 when a secondary (HT40) channel is set. If the C6 flags
the first 4 CSI bytes as invalid (a hardware limitation), they are written
as 0.

Schema is fixed at the hub (csv header written once per node file); adding a
receiver adds a file, never a column.

## Configuration

All settings are typed over each board's serial console and stored in NVS
(`wifi_sensing` namespace). Every board runs the same firmware for its role.

| Group     | Keys                                   | Boards |
|-----------|----------------------------------------|--------|
| Home WiFi | `STASSID`, `STAPASS`                   | all    |
| Sensing   | `RATE`                                 | TX     |
| Reporting | `NODEID`, `MQTTHOST`, `MQTTPORT`, `MQTTTOPIC` | RX |

`SHOWCONFIG` prints the current values (the password only as "(set)");
`REBOOT` applies them.

**Sensing channel vs. reporting network.** The sensing and reporting
*settings* are separate groups, but with one radio per board the sensing
*channel* is the home WiFi's channel: a board can only listen on the channel
it is connected on. To choose the sensing channel independently, reporting
would need a second path (Ethernet, a second radio, or USB to a host). Only
the WiFi and MQTT sections of `firmware/rx/main/main.cc` would change; the
record format and hub stay the same.

## Adding a receiver (config only, no code)

1. Flash `firmware/rx` to a new board.
2. Over serial: `STASSID:`, `STAPASS:`, `NODEID: rx-2` (unique),
   `MQTTHOST: <laptop IP>`, then `REBOOT`.

The new board hears the existing TX immediately, and the hub writes
`data/rx-2.csv` as soon as its first record arrives.
