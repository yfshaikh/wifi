# Third-party attribution

This project forks the **ESP32 CSI Tool** by Steven M. Hernandez
(MoWiNG Lab, Virginia Commonwealth University):

- Repo: https://github.com/StevenMHernandez/ESP32-CSI-Tool
- Pinned commit: `cce5b1a055fae5b01b30d1d39c5cfa1e0c656696`
- License: MIT (see `LICENSE`)

The CSI Tool is itself research software. If you use it, please cite:

> S. M. Hernandez and E. Bulut, "Lightweight and Standalone IoT Based WiFi
> Sensing for Active Repositioning and Mobility," 2020 IEEE WoWMoM.
> https://www.people.vcu.edu/~ebulut/wowmom20-WiFiSensing.pdf

Code derived from it now lives in `firmware/rx/main/main.cc` and
`firmware/tx/main/main.cc`: the NVS setup, the serial settings console, and
the CSI callback and record layout (ported to the ESP32-C6 / ESP-IDF 5 CSI
API). The upstream SD-card, UDP-sender and clock-setting components were
dropped.

Modified/vendored upstream code retains its original MIT copyright notice:
Copyright (c) 2020 Steven M. Hernandez.