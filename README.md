# WiFi sensing over CSI with two ESP32s

A proof-of-concept WiFi sensing system: one ESP32 **transmits** a packet
stream, one or more ESP32s **receive** it, measure Channel State Information
(CSI) on every packet, and publish each CSI record over MQTT to a central
hub. The hub stamps arrival time and writes per-node CSV files whose schema
stays fixed as more receivers are added.

## Goal

Build a **WiFi sensing** proof of concept: use commodity ESP32 hardware to
detect motion in a room by reading how the WiFi channel changes as people move
through it. When a body moves between two radios, the reflections it creates
perturb the wireless channel; the Channel State Information (CSI) measured by
the receiver captures those perturbations, and a changing CSI trace is the
signal we ultimately want to turn into a motion/occupancy decision.

The project deliberately splits that pipeline into reusable pieces so it can
grow from a one-room demo into a small sensor network:

1. **Sensing link** — one ESP32 broadcasts a fixed, configurable packet
   stream; every receiver extracts CSI per packet.
2. **Reporting network** — each receiver publishes its CSI records over MQTT
   to a central hub, so the data is live-visible and easily recorded. Sensing
   and reporting settings are separate groups (see `docs/architecture.md` for
   how the single radio ties the sensing channel to the home WiFi's).
3. **Scale by configuration, not code** — every receiver runs identical
   firmware; a new receiver is added by giving it the WiFi details, a node ID
   and the broker address over its serial console (stored in NVS). The hub's subscription and on-disk
   format don't change as nodes are added.
4. **Detection lives above the data** — motion detection is deliberately not
   baked into the firmware. The hub writes clean, schema-fixed records so you
   can prototype the detector in Python (variance/Doppler on subcarrier
   amplitudes first, ML later) without re-flashing hardware.

This is a PoC for the data-collection and -scaling side; making an accurate
detector is the follow-on problem.

Built on the [ESP32 CSI Tool](https://stevenmhernandez.github.io/ESP32-CSI-Tool/)
(upstream commit `cce5b1a`, MIT — see `THIRD_PARTY.md`).

```
  TX ESP32 ── ESP-NOW broadcast, 802.11n HT20, 100/s ──▶ RX ESP32 (one or many)
  (fw: tx)    from fixed MAC 1a:00:00:00:00:01            (fw: rx) keeps TX frames only,
                                                           one CSI record per frame
                                                              │ MQTT csi/<node>/data
                          all boards + laptop on home WiFi    ▼
                                         laptop: mosquitto ─▶ csi_hub.py
                                         subscribes csi/+/data, stamps arrival,
                                         writes data/<node>.csv, live plot
```

Every board joins your home WiFi. That puts them all on the router's channel,
where the TX's ESP-NOW broadcasts reach every receiver directly, and lets the
receivers reach the laptop's broker through the router.

## Repository layout

```
firmware/rx/main/main.cc  receiver: the whole firmware, one commented file
firmware/tx/main/main.cc  transmitter: the whole firmware, one commented file
docs/signal-path.html   clickable diagram of the system
hub/csi_hub.py          central collector (subscribe, stamp, per-node CSV, live plot)
hub/mqtt_fake_tx.py     publishes synthetic CSI records (test without hardware)
hub/mosquitto.conf.example
```

See `docs/architecture.md` for the design, the record format, and how
sensing vs. reporting configuration stays independent.

## Prerequisites

- A **2.4 GHz** home WiFi network (the C6 has no 5 GHz radio).
- Two or more **ESP32-C6** dev boards. Flash and monitor through the board's
  **UART / CH343** USB port, not the native USB port: the console is on UART0,
  so the native port shows nothing.
- [PlatformIO Core](https://platformio.org/install) (`brew install platformio`)
- [mosquitto](https://mosquitto.org/) (`brew install mosquitto`) + Python 3
  (`pip install -r hub/requirements.txt`)

## Build & flash

The ESP32-C6 needs ESP-IDF 5.1+. The official `platformio/espressif32`
platform stalled before C6 support, so both projects pin the community
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork
(`55.03.312-1`, ESP-IDF **5.5.5**). The first build downloads the toolchain
(a few minutes, once).

Flash one board at a time; with a single board plugged in PlatformIO finds the
port itself. If upload can't connect, hold **BOOT** while it says *Connecting…*.

```bash
# receiver
cd firmware/rx && pio run -t upload && pio device monitor --echo   # 921600 baud

# transmitter
cd firmware/tx && pio run -t upload && pio device monitor --echo
```

Build defaults come from `sdkconfig.defaults` (packet rate, MQTT topic,
etc.). WiFi credentials are deliberately empty there; set them per board over
serial so they're never committed.

## Provisioning (config-only receiver scaling)

Each role's firmware is **identical on every board**. Everything per-board is
typed into the serial console (`pio device monitor --echo`) and stored in NVS,
so it survives reflashing:

```
# every board
STASSID:  <your 2.4 GHz WiFi name>
STAPASS:  <your WiFi password>      # stored, never printed back

# each receiver
NODEID:   rx-1                      # unique -> topic csi/rx-1/data, column node_id
MQTTHOST: <laptop's IP on the WiFi> # where mosquitto runs: ipconfig getifaddr en0

# transmitter
RATE:     100                       # packets/s, applies live

SHOWCONFIG                          # print current values
REBOOT                              # apply
```

On boot the TX logs `sending as 1a:00:00:00:00:01`; each RX logs
`joined "<network>": ip ..., channel N`. All boards should report the same
channel.

**Add a receiver:** flash `firmware/rx`, set `STASSID`, `STAPASS`, a new
`NODEID` and `MQTTHOST`, reboot. It hears the existing TX straight away, and
the hub picks it up automatically (subscribes `csi/+/data`, writes
`data/<node_id>.csv`).

Give the laptop a fixed address in your router (a DHCP reservation) so
`MQTTHOST` stays valid. Clock sync is out of scope; the hub's arrival time is
the reference.

## Run the hub

```bash
cd hub
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt

# broker (listens on 0.0.0.0:1883, anonymous)
mosquitto -c mosquitto.conf.example

# collector -> writes data/<node>.csv as records arrive
.venv/bin/python csi_hub.py

# with a live CSI heatmap + motion score (needs a display)
.venv/bin/python csi_hub.py --live
```

Test the whole hub pipeline with no hardware:

```bash
.venv/bin/python mqtt_fake_tx.py --node rx-1 --rate 50
```

The hub writes one CSV per node. The last column is the raw CSI
(`[I0 Q0 I1 Q1 ...]`, int8 I/Q pairs — 128 values of HT20 HT-LTF). All other
columns match the CSI tool's serial format plus `arrival_epoch` (hub clock),
`node_id`, and `seq` (per-board monotonic sequence). Schema never changes as
nodes are added; see `docs/architecture.md`.

## Motion detection

CSI is the raw material; the actual detection is deliberately not baked in.
The classic first approach: compute per-subcarrier amplitude
`sqrt(I^2 + Q^2)` from the `csi` column and watch variance over a rolling
window — a moving body creates a characteristic time-varying pattern. The
hub's `--live` plot does this: a heatmap of every subcarrier's amplitude over
the last 20 s, and a motion score (rolling std of gain-normalized amplitude,
averaged over subcarriers) that sits flat when the room is still and jumps
when someone crosses the link.

## Notes & limitations

- **Single radio.** Each RX senses and reports over its one WiFi radio, so
  the sensing channel is the home WiFi's channel. Sensing and reporting
  settings are separate groups, so moving reporting to Ethernet later would
  free the channel without changing the record format or hub.
- **QoS 0 + drop-when-disconnected.** Records are dropped, never buffered, so
  a dead broker costs you data but not memory.
- **Effective rate.** CSI at ~100+ Hz means hundreds of small MQTT messages/s;
  a local broker easily keeps up. At very high rates the WiFi RX task may drop
  CSI frames — that's upstream tool behavior, not a bug.
- **Clock sync is out of scope** by design; `seq` + `arrival_epoch` give you
  per-board ordering and a global timeline.

## Attributions

Forked from the [ESP32 CSI Tool](https://github.com/StevenMHernandez/ESP32-CSI-Tool)
(MIT). Details and the required citation are in `THIRD_PARTY.md`.