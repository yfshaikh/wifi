# Architecture

## Why the roles are inverted vs. a "normal" TX/RX pair

CSI is a measurement a WiFi device makes on the **frames it receives**. So the
node you point at the environment has to be the one *getting* traffic. The
ESP32 CSI Tool's canonical pairing encodes that:

- **TX = active station** (`firmware/tx`, fork of `active_sta`). It joins the
  RX's AP and loops `sendto("1\n", 192.168.4.1:2223)` at `PACKET_RATE` packets
  per second (default 100, live-changeable via the `RATE` serial command).
- **RX = active AP** (`firmware/rx`, fork of `active_ap`). The AP's WiFi
  driver measures CSI on every frame the station sends (no application-level
  socket needed), so the CSI callback fires at roughly the TX packet rate.

Because the RX is the AP, the sensing link *is* the network: the laptop (and
future receivers) connect to the same AP, which is what lets MQTT work with no
external router.

## Data flow

```
TX ESP32                RX ESP32                              laptop
--------                --------                              ------
UDP burst          PHY CSI callback                  
  "1\n"            builds CSV record:                         
                   node_id,seq,role,mac,            esp-mqtt   mosquitto
                   rssi,...,len,csi  ── QoS0 ───▶  topic      ──▶ csi_hub.py
                   publishes to                     csi/<node>/data
                                                     (on the RX's own AP net)
                                                    subscribes csi/+/data,
                                                    stamps arrival_epoch,
                                                    appends data/<node>.csv
```

MQTT topic: `<mqtt_topic>/<node_id>/data`. Topics are per-node, so the hub's
single subscription `csi/+/data` scales to any number of receivers.

## Record format (one CSV row per packet)

```
arrival_epoch,node_id,seq,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,
not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,
secondary_channel,local_timestamp,ant,sig_len,rx_state,real_time_set,
real_timestamp,len,csi
```

- `arrival_epoch` — hub wall clock (seconds, ms precision). The sync anchor.
- `node_id` — from per-board NVS config.
- `seq` — per-board monotonic counter, incremented per captured packet.
- `local_timestamp` — ESP32 PHY RX timestamp (`rx_ctrl.timestamp`).
- `real_timestamp` — ESP32 steady clock, seconds since boot.
- `csi` — raw complex CSI, `[I0 Q0 I1 Q1 ...]` int8 pairs: 128 values of
  HT20 HT-LTF (64 subcarriers). The RX AP is pinned to 802.11b/g/n at 20 MHz so
  every record has this shape. The C6's subcarrier order differs from the
  classic ESP32's and L-LTF isn't captured (esp-idf issue #14271).

ESP32-C6 notes: the C6's `rx_ctrl` has no `mcs`, `smoothing`, `not_sounding`,
`aggregation`, `stbc`, `fec_coding`, `sgi`, `ampdu_cnt`, or `ant` fields, so
those columns are always 0. The columns stay so the schema matches the
upstream CSI tool. `sig_mode` carries the C6's `cur_bb_format` code and
`bandwidth` is 1 when a secondary (HT40) channel is set. If the C6 flags the
first 4 CSI bytes as invalid (a hardware limitation), they are written as 0.

Schema is fixed at the hub (csv header written once per node file); adding a
receiver adds a file, never a column.

## Independent configuration of sensing vs. reporting

Two separate config blocks, both available from the RX serial console and
stored in NVS (`wifi_sensing` namespace):

| Concern             | Keys                                        |
|---------------------|---------------------------------------------|
| Sensing channel     | `APSSID`, `APPASS`, `APCH` (RX = AP), `STASSID`, `STAPASS`, `STACH`, `RATE` (TX) |
| Reporting network   | `NODEID`, `MQTTHOST`, `MQTTPORT`, `MQTTTOPIC` |

They're decoupled even though today they ride the same radio: point
`MQTTHOST` at any broker reachable on that network, or later move reporting to
wired Ethernet with no firmware changes (only `mqtt_component.h`/`main.cc`
plumbing).

## Adding a receiver (config only, no code)

1. Flash the same `firmware/rx` image to a new board.
2. `NODEID: <unique>`, `MQTTHOST: <broker>`, optionally a distinct
   `APSSID`/`APCH`, then `REBOOT`.
3. Flash a `firmware/tx` image for that link if it doesn't share one already
   (a transmitter is a station and can only follow one AP at a time).

Scaling note: one TX serves one RX because a station associates with a single
AP. If you later want many RX nodes per TX, switch RXs to the tool's
`passive` fork (stations sniffing one TX AP's downlink) — that's a firmware
swap, not a hub change.