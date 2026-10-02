# WiFi sensing over CSI with two ESP32s

A proof-of-concept WiFi sensing system: one ESP32 **transmits** a packet
stream, a second ESP32 **receives** it, measures Channel State Information
(CSI) on every packet, and publishes each CSI record over MQTT to a central
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

1. **Sensing link** — one ESP32 transmits a fixed, configurable packet stream;
   a second ESP32 receives it and extracts CSI per packet. The two radios are
   dedicated to the sensing channel and are fully controllable (channel,
   packet rate).
2. **Reporting network** — the receiver publishes each CSI record over MQTT to
   a central hub, so the data is live-visible and easily recorded. The sensing
   channel and the reporting configuration are independent by design.
3. **Scale by configuration, not code** — every receiver runs identical
   firmware; a new receiver is added by giving it a node ID and broker address
   over its serial console (stored in NVS). The hub's subscription and on-disk
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
                ┌─────────────┐      sensing link (2.4 GHz)       ┌─────────────┐
   TX ESP32 ───▶│  station    │ ─────── UDP burst @ 100 pkt/s ───▶│  access   │◀──┐
   (fw: tx)     │  esp32      │   (packets the TX sends)          │  point    │   │  MQTT
                └─────────────┘                                   │  esp32     │───┤  <topic>/<node>/data
   laptop:                        joins the same WiFi network ◀───│ (fw: rx)   │   │
   mosquitto + csi_hub.py ──────────────────────────────────────────────────────┘   │
                                                                                      │
   hub subscribes csi/+/data, stamps arrival, writes data/<node>.csv ◀───────────────┘
```

Key point about roles: in the CSI tool's convention the **transmitter is the
station** and the **receiver is the access point**. The AP gets CSI from the
station's uplink packets, so the RX (detector) is the AP — which conveniently
lets the laptop and any future receivers all sit on the same sensing network.

## Repository layout

```
firmware/_components/   shared header components (vendored upstream + new)
firmware/rx/            PlatformIO project: receiver/AP  -> publishes CSI over MQTT
firmware/tx/            PlatformIO project: transmitter/station -> bursts packets
hub/csi_hub.py          central collector (subscribe, stamp, per-node CSV, live plot)
hub/mqtt_fake_tx.py     publishes synthetic CSI records (test without hardware)
hub/mosquitto.conf.example
```

See `docs/architecture.md` for the design, the record format, and how
sensing vs. reporting configuration stays independent.

## Prerequisites

- Two **ESP32-C6** dev boards. Flash and monitor through the board's
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
# receiver (detector/AP)
cd firmware/rx && pio run -t upload && pio device monitor --echo   # 921600 baud

# transmitter
cd firmware/tx && pio run -t upload && pio device monitor --echo
```

Build defaults come from `sdkconfig.defaults` (SSID, channel, packet rate,
MQTT, etc.). To tweak at build time: `pio run -t menuconfig`.

## Provisioning (config-only receiver scaling)

The firmware is **identical on every board**. Everything per-board is
configured at runtime over the serial console and stored in NVS — no code
changes, no re-flash:

```
NODEID:   rx-1          # unique node ID  -> topic csi/rx-1/data, column node_id
MQTTHOST: 192.168.4.2   # where mosquitto runs (the laptop, on the sensing network)
MQTTPORT: 1883
MQTTTOPIC: csi
APSSID:   wifi-sense-rx  # sensing network name (RX is the AP)
APPASS:   wifi-sense-123
APCH:     6               # sensing channel; TX (STACH) must agree
SHOWCONFIG                 # print current values
REBOOT                     # apply
```

Transmitter (station) keys: `STASSID`, `STAPASS`, `STACH` (0 = auto-scan),
`RATE` (packets/s, applies live). `SETTIME: <sec>.<usec>` is also available
from upstream but clock sync is out of scope — the hub's arrival time is the
reference.

**Add a receiver:** flash a board with the same `csi-rx` firmware, give it a
new `NODEID`, point it at the broker, reboot. The hub picks it up
automatically (subscribes `csi/+/data`, writes `data/<node_id>.csv`).

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
when someone crosses the link. It plots only the node's dominant source MAC
(the TX), since other stations on the AP also produce CSI.

## Notes & limitations

- **Single radio.** The RX senses and reports over the one ESP32 WiFi radio.
  In the PoC the MQTT traffic rides the same sensing network; sensing-channel
  and MQTT settings are separate config blocks so this can change later
  (e.g. wired Ethernet on the RX) with zero firmware surgery.
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