# Shared firmware components

Header-only components shared by the `tx` and `rx` PlatformIO projects.

## Upstream (MIT, ESP32-CSI-Tool)

`nvs_component.h` and `time_component.h` are unmodified copies from the
ESP32 CSI Tool. `csi_component.h`, `input_component.h`, `sockets_component.h`
are forks of upstream files, adapted for this project (ESP32-C6 / IDF 5 CSI
API, CSI publish-to-MQTT, serial config console, runtime packet rate).
Upstream's `sd_component.h` was dropped: SD recording was off by default and
used an SDSPI API that IDF 5 removed. MQTT to the hub is the recording path.

Upstream: https://github.com/StevenMHernandez/ESP32-CSI-Tool
Pinned commit: `cce5b1a055fae5b01b30d1d39c5cfa1e0c656696`
License: MIT (see repo LICENSE). See THIRD_PARTY.md.

## This project

- `config_component.h` — NVS-backed per-board config with Kconfig defaults.
  This is what makes "add a receiver by configuration only" work.
- `mqtt_component.h` — MQTT publisher to `<topic>/<node_id>/data` (QoS 0).