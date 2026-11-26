# ESP32-Specific Wi-Fi CSI Sensing Work

Scope: published and open-source work that uses ESP32-family MCUs (ESP32, S2, S3, C3, C5, C6) for Wi-Fi CSI sensing, written for the RIDE Fall 2026 Group 6 project. That project uses ESP32-C6 boards: one transmitter sending ESP-NOW at 100 Hz (HT20, MCS0), N promiscuous receivers capturing HT-LTF CSI (amplitude only), MQTT to a laptop hub, and no clock sync. Research date: 2026-11-05. Where a page could not be fetched (MDPI, ScienceDirect, PMC, and dblp blocked the fetcher), the details come from search-engine snippets and are marked as such.

## 1. ESP32 CSI Tool (Hernandez & Bulut, VCU) and follow-up work by the group

### Takeaway
The ESP32 CSI Tool paper is confirmed: Hernandez & Bulut, "Lightweight and Standalone IoT Based WiFi Sensing for Active Repositioning and Mobility," IEEE WoWMoM 2020, pp. 277–286. The group followed it with the IEEE COMST 2023 survey/system paper "WiFi Sensing on the Edge" (on-device ESP32 inference), an ICC 2021 adversarial one-sided through-wall occupancy paper, the WiFederated paper (IEEE IoT-J 2022), a PerCom 2023 countermeasure paper, and Hernandez's 2023 PhD dissertation.

### Cited Findings
- **ESP32 CSI Tool paper.** S. M. Hernandez and E. Bulut, "Lightweight and Standalone IoT Based WiFi Sensing for Active Repositioning and Mobility," IEEE WoWMoM 2020, Cork, Ireland (Aug 31–Sep 3, 2020), pp. 277–286. — [dblp](https://dblp.org/rec/conf/wowmom/HernandezB20); [researchr](https://researchr.org/publication/HernandezB20-2); [author PDF](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf)
- The paper builds a low-cost, standalone CSI collector on the ESP32 using ESP-IDF, so custom programs can run on the device. It argues that earlier CSI receivers (Intel 5300 or Atheros NICs on single-board computers) were costly and bulky, which made it hard to study repositioning and mobility of the receiver. Its experiments extract context such as the direction and depth of an activity for HAR, and it argues that accounting for receiver repositioning and mobility matters for accuracy. — [author PDF](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf) (summary from search snippet; specific accuracy figures were not retrieved)
- **Edge system and survey.** S. M. Hernandez and E. Bulut, "WiFi Sensing on the Edge: Signal Processing Techniques and Challenges for Real-World Systems," *IEEE Communications Surveys & Tutorials*, vol. 25, no. 1, pp. 46–76, 2023, DOI 10.1109/COMST.2022.3209144. The paper surveys signal-processing techniques, builds a low-cost ESP32 sensing system, and evaluates each method on board the ESP32 (computation time per method, overall sample throughput). It supports online ML through on-device inference. — [researchr BibTeX](https://researchr.org/publication/HernandezB23/bibtex); [AMiner](https://www.aminer.org/pub/632f108f90e50fcafd58f7b6/wifi-sensing-on-the-edge-signal-processing-techniques-and-challenges-for-real)
- **Adversarial occupancy.** S. M. Hernandez and E. Bulut, "Adversarial Occupancy Monitoring using One-Sided Through-Wall WiFi Sensing," IEEE ICC 2021 (Montreal). It covers device-free occupancy monitoring and crowd counting from one side of a wall (an attacker with access to only one room or the building exterior). — [author PDF](https://ebulutvcu.github.io/ICC21_Adversarial_Sensing.pdf); [researchr](https://researchr.org/publication/HernandezB21)
- **WiFederated.** Federated learning for WiFi sensing, where clients train in parallel at the edge. *IEEE Internet of Things Journal*, 2022, vol. 9, no. 14. — [author PDF](https://ebulutvcu.github.io/FL-IoT-journal-2021.pdf) (exact title wording and page numbers not confirmed; the search snippet gave only "WiFederated")
- **Countermeasure.** "Scheduled Spatial Sensing against Adversarial WiFi Sensing," IEEE PerCom 2023, pp. 91–100 (same authors). — [catalog record](https://vufind-test.katalog.k.utb.cz/SummonRecord/FETCH-LOGICAL-i204t-f4f4ed9e4fa5930a746c8910b211fa695fdab50c6170b9e5d2209e1445e42333)
- **Dissertation.** S. M. Hernandez, "WiFi Sensing at the Edge: Towards Scalable On-Device Wireless Sensing Systems," PhD dissertation, Virginia Commonwealth University, 2023 (advisor Bulut). It includes a chapter on scalable sensing with edge-based federated learning. — [VCU Scholars Compass](https://scholarscompass.vcu.edu/etd/7268/)
- The group also has a PerCom Workshops 2020 paper (p393-hernandez). — [PDF](https://staff.itee.uq.edu.au/jaga/proceedings/percomworkshops2020/papers/p393-hernandez.pdf). Title UNVERIFIED (not fetched).
- A 2025 ACM survey's tool table lists the Intel 5300 at 30 subcarriers and 3 antennas, and the ESP32 CSI Tool at 64 subcarriers and 1 antenna. — [ACM Computing Surveys, DOI 10.1145/3705893](https://dl.acm.org/doi/10.1145/3705893)

### Inferences
- For this project's lineage: Hernandez & Bulut 2020 is the canonical citation for "the ESP32 as a standalone CSI sensor," and the COMST 2023 paper is the canonical citation for on-ESP32 signal processing and inference cost.
- The WiFederated and dissertation work frames scaling as a distributed-learning problem, not a synchronized multi-receiver sensing network.

### Gaps
- The WoWMoM 2020 accuracy figures and the tool's ESP32-CSI-Tool repo features (active_sta / active_ap / passive sniffing modes, typical packet rates) were not fetched in this session. From prior knowledge only: the tool supports AP, station and passive modes, and its GitHub repo is StevenMHernandez/ESP32-CSI-Tool. UNVERIFIED in this session.
- I could not get the full dblp author listing (bot-blocked), so other group papers may be missing.

## 2. Espressif esp-csi, chip support, and the ESP32-C6 L-LTF / 128-byte issue (esp-idf #14271)

### Takeaway
Espressif's esp-csi repo says every ESP32-series chip supports CSI, including ESP32, S2, C3, S3, C5, C6 and C61. It publishes no numeric presence or motion accuracy. On the ESP32-C6, the HT-LTF overwrites the L-LTF in the CSI buffer, so an HT20 frame returns 128 bytes (64 subcarriers × 2 bytes) instead of 256. This is esp-idf #14271, which is still open, and the documented subcarrier order is unreliable on the C6.

### Cited Findings
- **Chip support.** "All ESP32 series support CSI, including ESP32 / ESP32-S2 / ESP32-C3 / ESP32-S3 / ESP32-C5 / ESP32-C6 / ESP32-C61." — [espressif/esp-csi](https://github.com/espressif/esp-csi)
- **Per-packet metadata.** Each packet includes RSSI, RF noise floor, reception time and the antenna field of `rx_ctrl`. The repo also mentions BLE scanning to assist detection, on-device ML (AI instructions on the dual-core 240 MHz parts), and adding CSI to existing products by OTA update. — [espressif/esp-csi](https://github.com/espressif/esp-csi)
- **Three deployment modes.**
  - Router-based: one ESP32 plus a router that replies to its pings.
  - Device-to-device: two or more ESP32s.
  - Broadcast: one sender plus multiple ESP32 receivers (A/B/C in the diagram).

  The broadcast mode is described only qualitatively, as having the "highest detection accuracy and reliability." It is architecturally the same as the Group 6 design. — [espressif/esp-csi](https://github.com/espressif/esp-csi)
- **esp-radar.** In the repo, esp-radar is a folder of applications, not a standalone library:
  - `connect_rainmaker`: uploads CSI to the RainMaker cloud.
  - `console_test`: interactive capture and configuration, used with human-activity detection algorithms.
  - `wifi_sensing_demo`: motion and presence detection on the `esp_wifi_sensing` component, with on-site training, LED feedback and a Web Serial monitor.

  No accuracy numbers are given. — [espressif/esp-csi](https://github.com/espressif/esp-csi)
- **Practical notes from Espressif.** An external IPEX antenna beats the directional PCB antenna, and calibration or testing should be done in an unoccupied room. — [espressif/esp-csi](https://github.com/espressif/esp-csi)
- **Coverage gap noted by the press.** A tech-press write-up noted that details about range, accuracy and multi-board use were hard to find. — [CNX Software, 2022](https://www.cnx-software.com/2022/08/08/esp-wifi-csi-detects-humans-with-wifi-signals-only-no-sensor-needed/)
- **esp-idf #14271.** Title: "ESP32-C6 not reporting L-LTF data in WiFi CSI callback and wrong subcarrier order for HT-LTF (IDFGH-13358)." Opened 2024-07-30 on ESP-IDF v5.3 with an ESP32-C6-DevKitC-1.
  - For an HT20 PPDU the callback returned 128 bytes instead of 256, and the L-LTF was missing.
  - The subcarrier order differed from the API guide.
  - The ESP32 and ESP32-S3 behaved as documented.
  - Some later samples contained unexpected zeros or were malformed.

  — [esp-idf #14271](https://github.com/espressif/esp-idf/issues/14271)
- **Espressif's replies on #14271.**
  - 2024-08-28: C6 CSI differs from C3. On C6 the HT-LTF overrides the L-LTF, with index range −32..−1 and 0..32 (effective −28..−1 and 1..28), and the docs were acknowledged as inaccurate.
  - 2024-09-04: invalid CSI can reach the callback. Use the `rx_channel_estimate_info_vld` field (in `esp_wifi_rxctrl_t`, `esp_wifi_he_types.h`) to filter it.
  - 2025-03-31: on C6 (MAC v3), configure CSI with `wifi_csi_acquire_config_t`. Its fields are `acquire_csi_legacy`, `acquire_csi_force_lltf`, `acquire_csi_ht20`, `acquire_csi_ht40`, `acquire_csi_vht`, `acquire_csi_su`, `acquire_csi_mu`, `acquire_csi_dcm`, `acquire_csi_beamformed`, `acquire_csi_he_stbc_mode`, `val_scale_cfg` and `dump_ack_en`.

  The issue was still open, with no code fix, when fetched. — [esp-idf #14271](https://github.com/espressif/esp-idf/issues/14271)
- **ESP-IDF docs.** The classic-MAC (ESP32-S2/C3) Wi-Fi Vendor Features table lists 128 total bytes for a non-HT L-LTF frame with no secondary channel. One third-party Rust crate says the C6 has no published CSI layout table. — [ESP-IDF Wi-Fi Vendor Features (ESP32-S2)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/wifi-driver/wifi-vendor-features.html); [esp-csi-rs layout.rs](https://docs.rs/crate/esp-csi-rs/0.13.0/source/src/lib/wire/layout.rs)

### Inferences
- Group 6's 64-subcarrier / 56-usable HT-LTF capture on the C6 matches Espressif's explanation (effective −28..−1, 1..28 = 56 subcarriers). The project should cite #14271 to justify using HT-LTF only, and to explain why no L-LTF is available for cross-checking.
- Filtering on `rx_channel_estimate_info_vld` is a directly applicable data-quality step for C6 receivers.
- Espressif's broadcast mode (one TX, many RX) is the vendor-sanctioned version of the Group 6 topology. Since Espressif publishes no accuracy or scaling data for it, that is a gap the project can fill.

### Gaps
- The C5's HE-LTF behavior (802.11ax, 5 GHz) was not verified in this session.
- I found no Espressif-published accuracy figures for presence or motion detection.
- The sampling-rate ceilings per chip were not documented on the pages I fetched.

## 3. Academic papers using ESP32 CSI: tasks, setups, accuracy

### Takeaway
Peer-reviewed ESP32 CSI work covers HAR (including long-range through-wall), respiration, localization and fingerprinting, and occupancy. Reported accuracies run from about 87% to 95% for classification tasks. Most setups use 1–4 boards in a single room or lab. Datasets exist (Wallhack1.8k, the Wong et al. Data in Brief dataset, and a 312-hour ESP32-C3 mesh dataset). I found no controlled head-to-head accuracy benchmark of the ESP32 against the Intel 5300.

### Cited Findings
**Activity recognition / through-wall**
- J. Strohmayer and M. Kampel (TU Wien), "WiFi CSI-Based Long-Range Through-Wall Human Activity Recognition with the ESP32," *Computer Vision Systems* (ICVS 2023), Springer LNCS, pp. 41–50, 2023. — [TU Wien repositum](https://repositum.tuwien.at/handle/20.500.12708/190653) (details from search snippet)
- J. Strohmayer and M. Kampel, "Directional Antenna Systems for Long-Range Through-Wall Human Activity Recognition," arXiv:2401.01388 (Jan 1, 2024). The arXiv page links DOI 10.1109/ICIP51287.2024.10647666 (ICIP 2024).
  - Hardware: four ESP32-S3 2.4 GHz directional antenna systems were evaluated. The two proposed were an ESP32-S3 with a biquad antenna, and the built-in PIFA with a plane reflector.
  - Setup: an office spanning 18 m across five rooms, in LOS and NLOS conditions. Models used EfficientNetV2.
  - Dataset: Wallhack1.8k, 1,806 CSI amplitude spectrograms, public.
  - NLOS accuracy: 92.0 ± 3.5% (biquad) and 86.8 ± 4.7% (PIFA with reflector).

  — [arXiv 2401.01388](https://arxiv.org/abs/2401.01388)
- **Dataset (Wong et al.).** W. E. Wong, A. H. Wong, W. Q. Peh, C. K. Tan, "A high-dimensional, multi-transceiver channel state information dataset for enhanced human activity recognition," *Data in Brief*, vol. 55, 2024, DOI 10.1016/j.dib.2024.110673.
  - Hardware: four ESP32-S3-DevKitC-1 boards as transceiver pairs, 802.11n, channel 11, 2 m apart on 1.5 m tripods, in a controlled lab.
  - Content: 10 activities, 6 participants, 166 subcarriers reported, about 300–450 packets per sample and about 1,200 samples per activity.

  — [DOAJ](https://doaj.org/article/a4475ad316fd4daf8e478d3076223dda); [PMC11268129](https://pmc.ncbi.nlm.nih.gov/articles/PMC11268129) (details from search snippets; pages blocked)
- **On-device HAR.** A 2025 MDPI *Sensors* article reports a compact DenseNet HAR model on an ESP32-S3 with 92.43% accuracy at 232 ms inference latency. — [search snippet; title, authors and DOI UNVERIFIED](https://www.mdpi.com/1424-8220/25/19/6220). Note: that URL is the review "Tools and Methods for Achieving Wi-Fi Sensing in Embedded Devices," *Sensors* 25(19):6220, 2025. It may be where the figure came from, but the attribution is UNVERIFIED.
- **Embedded review.** "Tools and Methods for Achieving Wi-Fi Sensing in Embedded Devices," *Sensors*, 2025, 25(19), 6220. It notes that the Intel 5300 is legacy or discontinued, and that ESP32 setups commonly stream CSI to a more powerful computer for processing. — [MDPI](https://www.mdpi.com/1424-8220/25/19/6220). Authors not retrieved.

**Respiration**
- **VitalCSI.** "VitalCSI: Contactless Respiratory Rate Estimation Using Consumer-Grade Wi-Fi Channel State Information," *Sensors*, 2026, DOI 10.3390/s26010225. Two ESP32s (one AP, one station) at 2.4 GHz captured CSI amplitude while volunteers slept. Accuracy was lower below 12 breaths/min. — [DOI](https://doi.org/10.3390/s26010225) (authors not retrieved)
- **Respiration digital twin.** An ESP32 CSI respiration paper in *Computers in Biology and Medicine* (2023) on a respiration "digital twin" reports 92.3% BPM-estimation accuracy from raw ESP32 CSI, and 89.2% / 83.7% for binary / multi-class classification. — [ScienceDirect](https://www.sciencedirect.com/science/article/pii/S0010482523012908) (authors and exact title not retrieved; figures from search snippet)
- PulseFi (arXiv 2510.24744, 2025) reports a breathing-rate MAE of 0.09 bpm with 20 s windows. Whether it used ESP32 hardware is UNVERIFIED. — [arXiv](https://arxiv.org/pdf/2510.24744)

**Localization / fingerprinting / occupancy**
- **Testbed with four ESP32s.** D. Macedo, M. Loureiro, Ó. G. Martins, J. C. Sousa, D. Belo, M. Gomes, "From CSI to Coordinates: An IoT-Driven Testbed for Individual Indoor Localization," *Future Internet*, 17(9), 395, published 30 Aug 2025. Authors are from Instituto de Telecomunicações / Univ. Coimbra and NOS Inovação.
  - Setup: ESP32s at fixed known positions extract 2.4 GHz CSI and RSSI. Search snippets say four ESP32-WROOM-32E modules simulated a smart home.
  - Ablation: one ESP was removed at a time to measure each receiver's contribution.

  — [MDPI](https://www.mdpi.com/1999-5903/17/9/395); [IDEAS/RePEc](https://ideas.repec.org/a/gam/jftint/v17y2025i9p395-d1737965.html). Full text blocked, so accuracy numbers and the transport/synchronization method were not retrieved.
- **Fingerprinting preprint.** "Device-Free Indoor Localization with ESP32 Wi-Fi CSI Fingerprints: Grid-Based Regression, Feature Modeling, and Multi-Link Experiments," Preprints.org 202601.2378 (Jan 2026, not peer-reviewed). It reportedly reaches sub-meter median error in a single room with a three-link testbed. — [Preprints.org](https://www.preprints.org/manuscript/202601.2378)
- **Trilateration.** An arXiv paper (2605.15593, 2026) uses three ESP32 APs as anchors and an ESP32 station, reporting a trilateration RMSE of 1.180 m. This figure is from a search snippet and was not fetched. — [arXiv](https://arxiv.org/pdf/2605.15593)
- A 2026 Preprints.org paper (202601.2295) on ESP32 room-level occupancy estimation exists. Details UNVERIFIED. — [Preprints.org](https://www.preprints.org/manuscript/202601.2295)

**Fall detection / presence**
- **ResearchGate fall-detection paper.** "Wi-Fi Signal-Based Through-Wall Sensing for Human Presence and Fall Detection Using ESP32 Module" (circa 2022) reports 93% accuracy. Authors and venue UNVERIFIED (ResearchGate only). It may be the "Kumar et al." ESP32 NLOS presence/fall work cited elsewhere. — [ResearchGate](https://www.researchgate.net/publication/360375906_Wi-Fi_Signal-Based_Through-Wall_Sensing_for_Human_Presence_and_Fall_Detection_Using_ESP32_Module)

**ESP32 vs Intel 5300**
- **ESP-Fi HAR.** An Elsevier article titled roughly "ESP-Fi HAR: A low-power WiFi CSI dataset for Ad-Hoc IoT human activity recognition" (2026, likely *Ad Hoc Networks*, PII S1570870526000582) uses ESP32-C3 nodes. It reportedly claims about 10× lower cost and over 95% lower power than the Intel 5300. Authors and DOI are UNVERIFIED (page blocked). — [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S1570870526000582)

### Inferences
- Accuracy numbers are not comparable across papers, because they mix classification accuracy, BPM error and localization RMSE, and most come from single-room lab setups with few subjects.
- Long-range through-wall HAR (Strohmayer & Kampel) shows that antenna choice matters a lot (about 5 points between biquad and PIFA). This supports Espressif's advice to use external antennas.

### Gaps
- I found no paper that directly benchmarks ESP32 against Intel 5300 CSI quality on the same task and room.
- Sampling rates were not retrieved for most papers.
- I found no peer-reviewed ESP32-C6 CSI sensing paper. The C6 appears mostly in vendor docs, issues and hobby projects.

## 4. Phase-coherent multi-antenna ESP32 arrays (ESPARGOS)

### Takeaway
ESPARGOS (University of Stuttgart, Euchner, ten Brink et al.) is the main demonstration that ESP32 chips can be phase-synchronized. It uses eight ESP32-S2 chips sharing a 40 MHz reference clock, plus an on-PCB reference frame to resolve PLL phase ambiguity. Multiple boards can be combined with a shared clock and phase reference. This is the opposite end of the design space from unsynchronized multi-receiver setups like Group 6's.

### Cited Findings
- **Channel-sounder paper.** F. Euchner, T. Schneider, M. Gauger, S. ten Brink, "ESPARGOS: An Ultra Low-Cost, Realtime-Capable Multi-Antenna WiFi Channel Sounder," arXiv:2502.09405, submitted 13 Feb 2025 (eess.SP). Target applications are HAR, presence detection, user localization and channel charting. — [arXiv](https://arxiv.org/abs/2502.09405)
- **Dataset paper.** F. Euchner and S. ten Brink, "ESPARGOS: Phase-Coherent WiFi CSI Datasets for Wireless Sensing Research," arXiv:2408.16377, 29 Aug 2024. The public datasets are labeled with reference positioning systems. — [arXiv](https://arxiv.org/abs/2408.16377)
- **Hardware and operation.**
  - Hardware: eight antennas in a 2×4 layout, each with its own ESP32-S2 (ESP32-S2FH4 chips), and circularly polarized patch antennas.
  - Synchronization: a shared 40 MHz reference clock gives frequency sync, and a Wi-Fi frame distributed over PCB microstrip lines of known length resolves the PLL phase ambiguity. Several units can be joined with a common clock and phase reference.
  - Operation: a passive sniffer that delivers L-LTF and HT-LTF CSI in real time. pyespargos currently supports HT40 only.

  — [arXiv 2408.16377 PDF](https://arxiv.org/pdf/2408.16377); [pyespargos via ecosyste.ms](https://awesome.ecosyste.ms/projects/github.com%2FESPARGOS%2Fpyespargos)
- One article says the hardware design and firmware are not open source, and that the creator plans to commercialize the device. — [CircuitDigest](https://circuitdigest.com/news/espargos-an-esp32-based-wifi-sensing-array-that-can-see-wireless-signals) (secondary source)

### Inferences
- ESPARGOS shows that phase-based sensing (AoA, channel charting) on ESP32s needs a hardware clock and phase distribution. Free-running boards like Group 6's are restricted to amplitude features and coarse, arrival-time-level alignment. That justifies the project's amplitude-only design choice.

### Gaps
- Peer-reviewed venues for the two ESPARGOS arXiv papers were not confirmed.

## 5. Known limitations of ESP32 CSI

### Takeaway
The limitations documented in what I could fetch are:
- single antenna and few subcarriers compared with multi-antenna NICs;
- 2.4 GHz only on most parts;
- invalid or malformed CSI frames and undocumented subcarrier layouts (C6);
- antenna directionality;
- sensitivity to people moving during calibration;
- no phase coherence across boards without shared clocks.

The standard phase-impairment list (CFO/SFO/STO, AGC) is well established in general CSI literature, but I did not verify an ESP32-specific quantitative characterization in this session.

### Cited Findings
- **Single antenna.** The ESP32 has one antenna versus three on the Intel 5300. — [ACM survey 10.1145/3705893](https://dl.acm.org/doi/10.1145/3705893)
- **Invalid CSI and layout problems.** Invalid CSI can be delivered to the callback, and the C6 subcarrier layout departs from the docs. — [esp-idf #14271](https://github.com/espressif/esp-idf/issues/14271)
- **Antenna and environment.** The PCB antenna is directional and an external antenna performs better. Movement during calibration corrupts it. — [esp-csi](https://github.com/espressif/esp-csi); antenna effect quantified in [Strohmayer & Kampel, arXiv 2401.01388](https://arxiv.org/abs/2401.01388)
- **Cross-board coherence.** Phase coherence across ESP32 chips needs a shared reference clock and a phase reference signal. — [ESPARGOS arXiv 2408.16377](https://arxiv.org/pdf/2408.16377)
- **Consumer-device CSI generally.** Consumer devices often lack spatial diversity and phase synchronization. — [Euchner & ten Brink 2024](https://arxiv.org/abs/2408.16377)
- **Respiration at low rates.** Respiration accuracy degrades at low breathing rates (below 12 bpm) with ESP32 amplitude CSI. — [VitalCSI, Sensors 2026](https://doi.org/10.3390/s26010225)

### Inferences
- Because Group 6 timestamps CSI at the MQTT hub, network jitter is added on top of on-board timing jitter. Cross-receiver fusion should use windowed features (for example 1 s variance), not sample-level alignment. This is an inference and was not tested in the sources.

### Gaps
- I found no ESP32-specific measurement study of CFO/SFO/STO, AGC steps, packet-loss rates or sampling-rate jitter. That would need a targeted search, and the area may genuinely be under-documented.
- I found no published data on clock drift between ESP32 boards.

## 6. Open-source and hobbyist projects (not peer-reviewed)

### Takeaway
There are many ESP32 CSI hobby projects: Espressif's own demo, Home Assistant presence builds, Hackster DIY builds, and RuView's "WiFi DensePose" claims. They are useful as engineering references but are self-reported. RuView's own README shows its live ESP32 pose results are far below its headline claims.

### Cited Findings
- **RuView (ruvnet/RuView).**
  - Hardware: recommends ESP32-S3 meshes of 3–6 nodes. The C6 is listed as a research node pending validation, and the original ESP32 and C3 are listed as unsupported.
  - Claims: presence, people counting, vital signs, falls and 17-keypoint pose.
  - Self-reported results: presence 82.3% held-out (a retracted earlier "100%" figure came from single-class data). Its MM-Fi pose benchmark (82.69% torso-PCK@20) is separate from the ESP32 pipeline. Its live CSI-only pose reaches only PCK@20 3.0% and PCK@50 18.5% on 217 samples, below the project's own target.
  - Status: beta, with no peer-reviewed publication.

  — [GitHub RuView](https://github.com/ruvnet/RuView)
- **Hackster DIY build.** A single TX/RX pair senses a "line," not a field, so the build used 4 XIAO ESP32 boards to cover blind spots. — [Hackster.io](https://www.hackster.io/limengdu0117/esp-csi-diy-wifi-human-presence-detection-f80508)
- **Home Assistant.** A community ESP32 CSI presence project exists. — [Home Assistant Community](https://community.home-assistant.io/t/human-presence-using-wifi-sensing-csi-on-esp32/791452)
- **ESP32-S3 presence project.** An empty-room vs one-person project with planned multi-node support. — [GitHub sudeep-embedded/wifi-human-presence](https://github.com/sudeep-embedded/wifi-human-presence)
- **Fall-detection project.** A single ESP32 plus router with a Random Forest reached 84.25% (3 classes: empty, falling, walking) and 70.00% (9 classes), self-reported. — [GitHub BhuvanB1405](https://github.com/BhuvanB1405/wifi-csi-activity-fall-detection)

### Inferences
- The project report should cite hobby projects only as evidence of community interest and design patterns (multi-node coverage), never for accuracy.

### Gaps
- I did not survey ESPHome or other Home Assistant integrations in depth.

## 7. Scalable multi-receiver ESP32 CSI networks (one TX, many RX; MQTT or similar)

### Takeaway
I found no peer-reviewed paper describing a scalable one-transmitter, many-receiver ESP32 CSI network with MQTT transport that reports on synchronization and scaling. The closest work points the other way:
- the 9-node / 72-link ESP32-C3 mesh study found that more links diluted performance;
- a four-ESP32 localization testbed did per-receiver ablations;
- Espressif documents a broadcast one-TX / multi-RX mode without numbers.

### Cited Findings
- **Dilution-effect study.** K. Khamaisi and B. Rodrigues, "Less is More: The Dilution Effect in Multi-Link Wireless Sensing," arXiv:2602.10823 (v1 11 Feb 2026, v3 3 Mar 2026; preprint).
  - Setup: a 9-node ESP32-C3 mesh with 72 links, deployed for 12 days in a residence for presence/activity (motion) sensing.
  - Results:
    - One well-placed link beat the full mesh: AUC 0.541 vs 0.489 (Cohen's d = 0.86).
    - Random link selection matched optimized selection (p = 0.35).
    - Link placement mattered 2.7× more than the choice of classifier.
  - Explanation offered: links whose Fresnel zones miss activity areas add noise.
  - Released: 312 hours of labeled CSI, firmware and code.

  Note that both AUCs are near chance. — [arXiv](https://arxiv.org/abs/2602.10823)
- **Possible dataset.** A Zenodo dataset (Feb 2026) from a 9-node ESP32-C3 mesh in a 5×4 m residence is likely the same team's release (inferred, not confirmed). — [Zenodo 18486250](https://zenodo.org/record/18486250)
- **Four-receiver testbed.** Macedo et al. 2025 used four fixed ESP32 receivers with per-ESP ablation. — [Future Internet 17(9):395](https://www.mdpi.com/1999-5903/17/9/395)
- **Vendor broadcast mode.** One sender with receivers A/B/C, documented without metrics. — [esp-csi](https://github.com/espressif/esp-csi)
- **Federated scaling.** Hernandez's work treats scaling through edge federated learning, not synchronized receiver networks. — [WiFederated](https://ebulutvcu.github.io/FL-IoT-journal-2021.pdf); [dissertation](https://scholarscompass.vcu.edu/etd/7268/)

### Inferences
- Gap for Group 6: a documented one-TX (ESP-NOW broadcast, fixed MAC, 100 Hz) / N-RX ESP32-C6 network with hub-side timestamps that reports per-receiver packet loss, inter-receiver timing skew, and accuracy as a function of receiver count and placement appears to be unpublished. This is a defensible contribution.
- The dilution result suggests the project should report zone accuracy per receiver subset (an ablation like Macedo et al.), not assume that more receivers are always better.

### Gaps
- I did not find whether Macedo et al. used MQTT or how they aligned the receivers. The full text was blocked, so check the PDF.
- I found no published measurements of MQTT or Wi-Fi backhaul latency jitter for CSI streaming from ESP32s.
