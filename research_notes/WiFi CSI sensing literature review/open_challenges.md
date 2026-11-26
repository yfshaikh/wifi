# Open Problems and Challenges in Wi-Fi CSI Sensing (for RIDE Fall 2026, Group 6 — ESP32-C6, 2.4 GHz/20 MHz, amplitude-only, single antenna, unsynchronized multi-RX over MQTT)

Notes compiled 2026-11-05. Verification legend: **[V]** = bibliographic details confirmed this session against a primary or catalog source (URL given). **[PV]** = paper confirmed to exist, some details (pages, issue, exact year) not confirmed. **UNVERIFIED** = from background knowledge only; check before citing.

Core surveys used as anchors:
- [V] Y. Ma, G. Zhou, S. Wang, "WiFi Sensing with Channel State Information: A Survey," ACM Computing Surveys 52(3), DOI 10.1145/3310194 — [Crossref](https://api.crossref.org/works/10.1145%2F3310194); [author PDF](https://gzhou.pages.wm.edu/wp-content/blogs.dir/5736/files/sites/13/2017/12/WiFiSenseSurvey_CSUR19.pdf). Year note: author file is labelled "CSUR19"; one search summary called it 2020. Cite it as 2019 (vol. 52, issue 3); confirm the issue date on the DOI page.
- [V] S. Tan, Y. Ren, J. Yang, Y. Chen, "Commodity WiFi Sensing in Ten Years: Status, Challenges, and Opportunities," IEEE Internet of Things Journal 9(18):17832–17843, 2022, DOI 10.1109/JIOT.2022.3164569 — [NSF PAR](https://par.nsf.gov/biblio/10473796-commodity-wifi-sensing-ten-years-status-challenges-opportunities); [arXiv 2111.07038](https://arxiv.org/abs/2111.07038)
- [V] C. Chen, G. Zhou, Y. Lin, "Cross-Domain WiFi Sensing with Channel State Information: A Survey," ACM Computing Surveys 55(11), Nov 2023, pp. 1–37, DOI 10.1145/3570325 — [Bohrium record](https://www.bohrium.com/paper-details/cross-domain-wifi-sensing-with-channel-state-information-a-survey/831481475873374208-2425)
- [PV] "A Survey on Wi-Fi Sensing Generalizability: Taxonomy, Techniques, Datasets, and Future Research Prospects," arXiv 2503.08008 (v1 March 2025, revised through v4) — [arXiv](https://arxiv.org/abs/2503.08008v4). Authors not captured this session; get them from the arXiv page.
- [PV] S. M. Hernandez, E. Bulut, "WiFi Sensing on the Edge: Signal Processing Techniques and Challenges for Real-World Systems," IEEE Communications Surveys & Tutorials (ESP32-centred edge survey) — the title was confirmed in [search results](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf) and [COMST22 PDF mirror](https://www.ib.edu.ar/fisica-exp/images/e/e0/COMST22_WiFi_Sensing_Survey.pdf). Volume/issue/year (believed 2023, vol. 25, no. 1) **UNVERIFIED**. This is the most relevant survey for an ESP32 project.
- UNVERIFIED (not checked this session): Yousefi et al., "A Survey on Behavior Recognition Using WiFi Channel State Information," IEEE Communications Magazine 55(10), 2017; J. Liu et al., "Wireless Sensing for Human Activity: A Survey," IEEE COMST 22(3), 2020.

---

## 1. Environment dependence / domain shift / cross-domain generalization

### Takeaway
This is the main open problem in CSI sensing. CSI records the whole multipath channel, so a model trained in one room, position, orientation, person or device does not carry over to another. Known fixes include domain-invariant features (BVP), adversarial training (EI), synthetic samples (WiAG, CrossSense), transfer learning or few-shot learning, and big-data approaches. Each fix needs something an ESP32-C6 amplitude-only, single-antenna setup lacks, such as multiple antennas, phase, several synchronized links, or a lot of labelled data. Generalization to an unseen environment is not solved.

### Cited Findings
- **Problem statement.** Chen, Zhou & Lin's cross-domain survey frames the problem as keeping a pre-trained system working when it is moved to a new domain (new environment, configuration, etc.). It sorts solutions into five families: domain-invariant feature extraction, virtual sample generation, transfer learning, few-shot learning, and big-data solutions. It reviews about 60+ papers — [Chen et al. 2023 via catalog](https://searchworks.jioinstitute.edu.in/articles/asx__162604185); [arXiv 2503.08008 citing Chen et al.](https://arxiv.org/html/2503.08008v1)
- **Three root causes.** The 2025 generalizability survey reviews more than 200 studies. It names three factors that block generalization: **device heterogeneity, human body diversity, and environmental diversity**. It organizes solutions along the pipeline: device deployment → signal processing → feature learning → model deployment — [arXiv 2503.08008](https://arxiv.org/html/2503.08008v1)
- **EI (adversarial domain removal)** [V]. W. Jiang et al., "Towards Environment Independent Device Free Human Activity Recognition," ACM MobiCom 2018, pp. 289–304, DOI 10.1145/3241539.3241548. The paper notes that a major unaddressed challenge is "the lack of environment/subject-independent features." EI is an adversarial network made of a feature extractor, an activity recognizer and a domain discriminator. It was evaluated on WiFi, ultrasound, 60 GHz mmWave and visible-light testbeds — [author PDF](https://cse.buffalo.edu/~lusu/papers/MobiCom2018.pdf); [dblp](https://dblp.org/rec/conf/mobicom/JiangMMYWYXSMKX18.html); [Springer DOI](https://www.springerprofessional.de/doi/10.1145/3241539.3241548). (The third component, a domain discriminator, comes from background knowledge. The snippet was cut off. Check it in the PDF.)
- **Widar3.0 / BVP (domain-independent physical feature)** [V]. Y. Zheng et al., "Zero-Effort Cross-Domain Gesture Recognition with Wi-Fi," ACM MobiSys 2019, DOI 10.1145/3307334.3326081. It introduces the body-coordinate velocity profile (BVP), the power distribution over velocities in the body's own coordinate frame. It reports 92.7% in-domain accuracy and 82.6–92.4% cross-domain accuracy (across environment, location and orientation) without retraining — [author PDF](https://ct.cswu.me/papers/mobisys19_widar3_paper.pdf); [IEEE DataPort dataset](https://ieee-dataport.org/open-access/widar30-dataset-zero-effort-cross-domain-gesture-recognition-wi-fi). A journal extension in IEEE TPAMI (2021/2022) is **UNVERIFIED**. Search found none.
- **WiAG (virtual samples via translation function)** [V]. A. Virmani, M. Shahzad, "Position and Orientation Agnostic Gesture Recognition Using WiFi," ACM MobiSys 2017, DOI 10.1145/3081333.3081340. It is trained in one configuration and generates virtual samples for every other configuration. At runtime it estimates the user's configuration and then classifies. When the runtime configuration differs from training, accuracy goes from 51.4% to 91.4% — [researchr](https://researchr.org/publication/VirmaniS17); [catalog](https://katalog.lib.cas.cz/KNAV/EdsRecord/edsair,edsair.doi...........0e0c22e590cac0bc443943a320ae3d56)
- **CrossSense (roaming model + mixture of experts)** [V]. J. Zhang, Z. Tang, M. Li, D. Fang, P. Nurmi, Z. Wang, "CrossSense: Towards Cross-Site and Large-Scale WiFi Sensing," ACM MobiCom 2018 (New Delhi), DOI 10.1145/3241539.3241570. An offline-trained "roaming model" generates synthetic training samples for each target site, and a mixture of experts handles larger problems. Accuracy rose from about 20% to over 80% (gait ID) and over 90% (gesture) — [Lancaster eprint](https://eprints.lancs.ac.uk/id/eprint/126425); [project page](https://zwang4.github.io/publication/mobicom18). The gains were reported in **controlled single-link** settings — [Lancaster eprint](https://eprints.lancs.ac.uk/id/eprint/126425)
- **Device heterogeneity is a separate domain axis.** Portner, Gringoli, Hollick & Asadi, "Same Signal, Different Story: Demystifying Receiver Effects in Wi-Fi Channel State Information" (arXiv 2605.26836, May 2026; listed as IEEE IoT Journal vol. 13, no. 10, 15 May 2026) fed identical precoded signals to several COTS receivers. They found **AGC to be the main source of cross-device differences**, along with consistent subcarrier nonlinearities. A gain-alignment preprocessing step recovers up to 75% of the accuracy lost in cross-device HAR — [arXiv 2605.26836](https://arxiv.org/abs/2605.26836)
- **Transferability measured in a benchmark.** SenseFi explicitly evaluates feature transferability and unsupervised-learning adaptability across WiFi platforms. This shows the field treats transfer as unsolved rather than settled — [arXiv 2207.07859](https://arxiv.org/abs/2207.07859v3)

### Inferences
- BVP, the flagship domain-independent feature, needs Doppler (DFS) estimates from several spatially separated links with usable phase. Widar3.0 used multiple Intel 5300 receivers. An amplitude-only, single-antenna ESP32-C6 cannot compute BVP as published. This is a direct gap for the group.
- EI-style adversarial training and few-shot or transfer learning are hardware-agnostic. They are the most realistic generalization levers for the group, at the cost of collecting data in at least two rooms or layouts.
- Cross-*receiver* shift (two ESP32-C6 boards giving different CSI for the same scene, from AGC and front-end differences) probably matters as much as cross-room shift in a multi-node ESP32 network. Per-board gain alignment is a cheap mitigation.

### Gaps
- No verified result found for leave-one-environment-out accuracy on ESP32-class (single-antenna, amplitude-only) hardware.
- Exact author list of arXiv 2503.08008 and the "Wang et al. 2023" generalization survey named in the brief were not verified. I could not confirm a 2023 survey by Wang et al. on this topic. Treat it as UNVERIFIED.

---

## 2. Stationary presence and respiration; Fresnel-zone blind spots

### Takeaway
Motion is easy to detect. A *still* person is hard, because they show up only as tiny periodic changes (breathing, mm-scale chest motion) or a static change in the multipath profile that slowly gets absorbed into the baseline. Fresnel-zone theory from Daqing Zhang's group shows that respiration sensitivity depends on where the person sits relative to the TX–RX link at centimetre scale, so "blind spots" exist. The published fixes (FarSense's CSI ratio, combining amplitude and phase) need two antennas and phase.

### Cited Findings
- [V] H. Wang, D. Zhang, J. Ma, Y. Wang, Y. Wang, D. Wu, T. Gu, B. Xie, "Human respiration detection with commodity WiFi devices: do user location and body orientation matter?" UbiComp 2016, pp. 25–36, Heidelberg, DOI 10.1145/2971648.2971744. The paper introduces the free-space Fresnel model, checks it for indoor WiFi propagation, and links breathing depth, location and orientation to whether respiration is detectable. The authors say it "sheds light on understanding the physical limit" of WiFi sensing — [dblp](https://dblp.dagstuhl.de/rec/conf/huc/WangZMWWWGX16.html); [IP Paris portal](https://researchportal.ip-paris.fr/en/publications/human-respiration-detection-with-commodity-wifi-devices-do-user-l/)
- [V] Companion UbiComp 2016 demo, "When can we detect human respiration with commodity WiFi devices?" (DOI 10.1145/2968219.2971394). It shows that **a centimetre-scale position change** affects detection performance, and that detectable and undetectable regions follow Fresnel-zone theory — [AMiner](https://www.aminer.org/pub/5843774eac44360f1083a122/when-can-we-detect-human-respiration-with-commodity-wifi-devices); [DOI](https://www.doi.org/10.1145/2968219.2971394)
- [V] Y. Zeng, D. Wu, J. Xiong, E. Yi, R. Gao, D. Zhang, "FarSense: Pushing the Range Limit of WiFi-based Respiration Sensing with CSI Ratio of Two Antennas," Proc. ACM IMWUT 3(3), 2019 (UbiComp 2019), DOI 10.1145/3351279. Dividing the CSI of two antennas cancels most of the noise and makes phase usable. Combining "orthogonal amplitude and phase information from the CSI ratio" avoids blind spots. Range is about 8 m (more than 100% beyond prior work; range defined as distance with >95% detection, where detection means within 0.5 bpm), and it works with one transceiver in another room — [arXiv 1907.03994](https://arxiv.org/pdf/1907.03994); [HAL](https://hal.archives-ouvertes.fr/hal-02321014)

### Inferences
- With amplitude only, the group sees only one projection of the complex CSI. Where the respiration-induced change is mostly in phase (a Fresnel "blind spot"), amplitude is near-insensitive. FarSense fixes this by pairing amplitude and phase from the CSI ratio, and a single-antenna ESP32-C6 cannot do the same. The practical workarounds are subcarrier diversity (different subcarriers have different wavelengths, so the blind spots fall in slightly different places), several RX nodes at different geometries (the multi-node MQTT network helps here), and moving nodes. Hernandez & Bulut reported accuracy gains of up to 29.4% / 28.2% from RX repositioning and mobility on ESP32 — [WoWMoM 2020 PDF](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf)
- Static-presence detection with adaptive baselines risks "learning away" a person who stays still. The group should state this limitation explicitly.

### Gaps
- No verified paper found that quantifies respiration detection rate on ESP32 amplitude-only CSI at 20 MHz. Treat as an open gap.
- Did not verify later Zhang-group work (e.g., "Fresnel zone" sensing-limit papers, WiBorder, or Diversity-of-CSI). UNVERIFIED if cited.

---

## 3. Phase sanitization on commodity hardware (and why single antenna rules out conjugate multiplication)

### Takeaway
Raw CSI phase on commodity NICs is corrupted by CFO, SFO, STO/packet-detection delay and PLL phase offsets that vary from packet to packet. The standard tools are linear detrending across subcarriers (removes STO/SFO slope plus offset), CSI ratio, and conjugate multiplication across antennas of the *same* NIC. The last two rely on all RF chains sharing one oscillator, so the random offsets are identical and cancel. A single-antenna receiver has nothing to divide or multiply by, so this whole family is unavailable.

### Cited Findings
- FarSense: the ratio of two antennas' CSI cancels most of the noise and "makes phase information usable, which a single antenna can't provide" — [arXiv 1907.03994](https://arxiv.org/pdf/1907.03994)
- IndoTrack and Widar2.0 rely on random phase offsets being identical across antennas of the same NIC, and use conjugate multiplication to remove the phase noise — secondary: [arXiv 2603.17283 survey](https://arxiv.org/pdf/2603.17283). Conjugate multiplication of signals from different antennas removes frequency offset — [DoDTrack arXiv 2608.06866](https://arxiv.org/pdf/2608.06866)
- A 2017 Peking University patent (inventors include Xiang Li, Daqing Zhang) describes conjugate-multiplying CSI from two antennas to get Doppler shift, then subtracting the mean to remove static paths. Chinese priority date 17 July 2017 — [patent text](https://ipiq.io/patents/4848605); [US app 20190020425](https://uspto.report/patent/app/20190020425)
- Phase from the Intel 5300 tool is described by users as unstable — [Linux 802.11n CSI Tool FAQ](https://dhalperi.github.io/linux-80211n-csitool/faq.html) (noted in search summary; check the FAQ wording)
- UNVERIFIED (background): X. Li, D. Zhang, Q. Lv, J. Xiong, S. Li, Y. Zhang, H. Mei, "IndoTrack: Device-Free Indoor Human Tracking with Commodity Wi-Fi," Proc. ACM IMWUT 1(3), 2017, Article 72. Linear phase detrending for sanitization is generally credited to Sen et al. (SpotFi / "You are facing the Mona Lisa", MobiSys 2012) and Kotaru et al. (SpotFi, SIGCOMM 2015). All UNVERIFIED this session.

### Inferences
- The group's amplitude-only choice is effectively forced. ESP32 CSI phase from one RF chain carries the per-packet random offsets. Linear detrending can remove the slope and offset, but residual CFO and PLL phase cannot be cancelled without a second, co-clocked chain. Across *different* ESP32 boards (each with its own oscillator), conjugate multiplication does not work either.
- Gap-analysis framing: "Doppler/velocity-based methods (Widar 2.0/3.0, IndoTrack) and ratio-based respiration (FarSense) are out of scope for single-antenna amplitude CSI. The project tests how far amplitude statistics alone can go."

### Gaps
- Did not verify the IndoTrack primary record or a definitive ESP32 phase-stability measurement study.

---

## 4. Automatic gain control (AGC) effects on CSI amplitude

### Takeaway
Commodity receivers rescale the received signal per packet with AGC. That introduces amplitude jumps unrelated to the channel, which corrupts amplitude-only features and is a main driver of cross-device shift. On ESP32, mitigation means reading the per-packet gain and compensating, or locking the gain. Espressif now ships an official component for this.

### Cited Findings
- AGC is the dominant source of cross-device CSI differences. A lightweight gain-alignment step recovers up to 75% of lost cross-device HAR accuracy. Receiver effects matter little for robust tasks (HAR) but a lot for high-precision tasks (single-shot ToF) — [Portner et al., arXiv 2605.26836](https://arxiv.org/abs/2605.26836). Per a search summary, the paper also notes that on ESP32 the gain-control effect "looks indistinguishable from noise" in plots — [arXiv HTML](https://arxiv.org/html/2605.26836) (not confirmed in the fetched abstract; check the full text)
- Espressif's **esp_csi_gain_ctrl** component compensates for AGC and FFT gain variations on ESP32, ESP32-S3 and other chips. It collects a gain baseline and computes a compensation factor from the difference between the current gain and that baseline — [Espressif component registry](https://components.espressif.com/components/espressif/esp_csi_gain_ctrl/versions/0.1.5/readme). (The supported-chip list was not checked for ESP32-C6 specifically. Verify.)
- CiUAV (arXiv 2505.21216, 2025) uses ESP32-S3 per-packet AGC gain. It converts dB to linear and multiplies each subcarrier amplitude by the reciprocal. Without this, AGC errors push fingerprints off the path-loss model — [arXiv 2505.21216](https://arxiv.org/pdf/2505.21216)
- Community practice includes fixing the AGC to a constant to test model stability. At least one forum poster disputes how much ESP32 amplitude is distorted, so this is not settled — [esp32.com AGC thread](https://esp32.com/viewtopic.php?p=112486); [esp32.com](https://esp32.com/viewtopic.php?p=149973)

### Inferences
- For an amplitude-only pipeline, uncompensated AGC can make a gain step look like motion. The group should log per-packet gain (where the C6 API exposes it), compensate or lock it, and use per-subcarrier normalization (e.g., dividing by mean amplitude across subcarriers), which cancels a common gain factor at the cost of losing absolute level.

### Gaps
- No peer-reviewed quantification of AGC impact specifically on ESP32-C6 was found.

---

## 5. Sampling rate, packet rate, irregular/lossy sampling, injected vs. ambient traffic

### Takeaway
CSI only exists when a frame is received, so the sensing sample rate equals the packet rate seen by the receiver. With ambient traffic that rate is bursty and irregular, and frames get lost. Most academic systems inject packets at fixed high rates (hundreds to 1000+ Hz on Intel 5300 setups) to get uniform sampling, which costs airtime. 802.11bf formalizes sensing-measurement scheduling partly because of this.

### Cited Findings
- 802.11bf defines sensing procedures (sensing measurement setup and instances, including trigger-based sounding) so that measurements are scheduled rather than opportunistic — [Du et al. overview, arXiv 2310.17661](https://arxiv.org/pdf/2310.17661); [Restuccia, arXiv 2103.14918](https://arxiv.org/abs/2103.14918)
- The ESP32-CSI-Tool supports active (injecting) and passive (sniffing) collection modes — [ESP32-CSI-Tool](https://relatedrepos.com/gh/StevenMHernandez/ESP32-CSI-Tool); [WoWMoM 2020 bib](https://raw.githubusercontent.com/StevenMHernandez/ESP32-CSI-Tool/master/docs/bibtex/esp32_csi_tool_wowmom.bib)
- [PV] An experience paper on scaling WiFi sensing to millions of commodity home devices (arXiv 2506.04322, June 2025) deals with deployment realities at scale — [arXiv 2506.04322](https://arxiv.org/html/2506.04322v1). Its content was not read this session. Check it for packet-rate and traffic findings before citing specifics.

### Inferences
- Nyquist reasoning: respiration (~0.1–0.5 Hz) needs only a few Hz, but walking or gesture Doppler at 2.4 GHz (λ≈12.5 cm; a 1 m/s reflector velocity gives a path-length Doppler of up to about 16 Hz) needs tens to roughly 100 Hz of *uniform* samples. Over MQTT, the group should timestamp at the receiver, resample or interpolate onto a uniform grid, and report the packet-loss rate. Self-injected traffic (e.g., ping/ESP-NOW at a fixed rate) gives more uniform sampling than ambient traffic but adds channel load. (Arithmetic inference, not a cited result.)

### Gaps
- I did not verify a specific paper that quantifies accuracy vs. packet rate or irregular sampling on ESP32. The SenseFi or Hernandez & Bulut COMST survey may contain this. UNVERIFIED.

---

## 6. Multi-link / multi-node sensing: synchronization, fusion, scalability, channel sharing

### Takeaway
Several links give spatial diversity, which helps against blind spots, localization and domain shift. But separate commodity receivers have independent clocks, so there is no phase coherence and only coarse time alignment. Fusion is usually done at the feature or decision level. Sensing traffic competes with data traffic for airtime. Fully coherent multi-receiver sensing on commodity hardware is unsolved. 802.11bf adds multi-STA sensing procedures but does not make cheap devices phase-coherent.

### Cited Findings
- Widar3.0's cross-domain results depend on BVP derived from multiple links (one transmitter, several receivers) — [Widar3.0 paper](https://ct.cswu.me/papers/mobisys19_widar3_paper.pdf); [dataset page](https://ieee-dataport.org/open-access/widar30-dataset-cross-domain-gesture-recognition-wi-fi)
- CrossSense's reported gains were in **single-link** settings, which implies multi-link generalization was outside its scope — [Lancaster eprint](https://eprints.lancs.ac.uk/id/eprint/126425)
- ESP32 RX repositioning/mobility raised accuracy by up to 29.4%/28.2% over a static RX. Spatial diversity matters — [Hernandez & Bulut, WoWMoM 2020](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf)
- 802.11bf adds sensing by sensing responders and multi-station measurement procedures, and its sub-7 GHz procedures build on existing sounding — [Du et al. arXiv 2310.17661](https://arxiv.org/pdf/2310.17661)

### Inferences
- The group's MQTT architecture means timestamps come from each node's clock or the broker's arrival time. Without NTP or PTP-grade sync, alignment is about tens of ms at best. That is fine for decision-level fusion (majority vote or a confidence-weighted presence decision) but rules out cross-node phase or Doppler fusion. This is a legitimate, citable gap: "low-cost unsynchronized multi-node fusion" is under-studied compared with single-link Intel 5300 work.
- More nodes on one 2.4 GHz channel raise contention. Sensing packets cut capacity and data traffic disturbs sampling. No verified quantitative source was found this session.

### Gaps
- No verified paper on clock-sync error vs. fusion accuracy for ESP32 networks, or on the airtime cost of sensing at scale. Mark as gap.

---

## 7. Benchmarks, datasets, reproducibility

### Takeaway
For years every paper used its own private testbed, hardware and protocol, so results could not be compared. Public datasets (Widar3.0, SignFi, UT-HAR, NTU-Fi, MM-Fi) and SenseFi's unified benchmark helped. But almost all of these use Intel 5300 or Atheros NICs with antenna arrays and phase, so they do not transfer to ESP32 amplitude-only data. Different subcarrier counts (30 for Intel 5300, 114 for Atheros at 40 MHz, 52–64 at 20 MHz on ESP32) also block direct reuse.

### Cited Findings
- [V] J. Yang, X. Chen, H. Zou, C. X. Lu, D. Wang, S. Sun, L. Xie, "SenseFi: A library and benchmark on deep-learning-empowered WiFi human sensing," *Patterns* 4(3):100703, 28 Feb 2023 (PubMed 36960448). It states that a comprehensive public DL benchmark for WiFi sensing comparable to vision "does not yet exist." It compares MLP/CNN/RNN/Transformer models on accuracy, model size, computation and transferability, using **NTU-Fi HAR, NTU-Fi HumanID, UT-HAR, and Widar** — [arXiv 2207.07859](https://arxiv.org/abs/2207.07859v3); [PubMed](https://www.pubmed.gov/36960448/); [GitHub](https://github.com/xyanchen/WiFi-CSI-Sensing-Benchmark). (The full author list beyond Yang and Chen comes from background knowledge. Check it on the Patterns page.)
- [V] Widar3.0 dataset is public (IEEE DataPort) and includes raw CSI, RSSI, DFS and BVP. The maintainers ask users to cite the MobiSys 2019 paper — [IEEE DataPort](https://ieee-dataport.org/open-access/widar30-dataset-zero-effort-cross-domain-gesture-recognition-wi-fi)
- The 2025 generalizability survey includes a dataset taxonomy — [arXiv 2503.08008](https://arxiv.org/html/2503.08008v2)
- UNVERIFIED this session (background): SignFi (Ma et al., IMWUT 2(1), 2018, 276 sign gestures, Intel 5300); UT-HAR (Yousefi et al., IEEE Commun. Mag. 2017); MM-Fi (Yang et al., NeurIPS 2023 Datasets & Benchmarks, multi-modal incl. WiFi CSI). Verify before citing.
- Hardware-specific data formats are fragmented enough that dedicated multi-format parsers exist (CSIKit: Atheros, Intel 5300, Nexmon, ESP32, PicoScenes) — [CSIKit](https://github.com/rack570/CSIKit)

### Inferences
- Gap the group can claim: few public datasets come from ESP32-class, single-antenna, 2.4 GHz/20 MHz hardware with multi-node layouts. Publishing their own data (with room layout, node positions, packet rates and AGC settings) would be a real contribution to reproducibility.

### Gaps
- Did not verify whether any public ESP32-C6 CSI dataset exists as of late 2026.

---

## 8. Standardization: IEEE 802.11bf

### Takeaway
IEEE 802.11bf-2025 ("Enhancements for Wireless LAN Sensing," Amendment 4) was approved 28 May 2025 and published 26 September 2025. It is an active standard as of late 2026. It defines MAC-level sensing procedures and reporting for sub-7 GHz and 60 GHz (DMG/EDMG). It standardizes how measurements are requested, scheduled and reported, but not the sensing algorithms or applications. Chip and AP support is only beginning. The ESP32-C6 (Wi-Fi 6) is pre-11bf and gets no benefit.

### Cited Findings
- 802.11bf-2025 is Amendment 4, "Enhancements for Wireless LAN Sensing." It was approved by the IEEE SA Standards Board on 28 May 2025, published 26 Sep 2025, and is listed as Active. It covers license-exempt bands 1–7.125 GHz and >45 GHz, modifying the MAC, the DMG/EDMG PHYs and the HE/EHT PHY service interface — [ANSI webstore](https://webstore.ansi.org/standards/ieee/ieee80211bf2025); [mystandards listing](https://www.mystandards.biz/standard/ieee-802-11bf-2025-26.9.2025.html); [DIN Media](https://www.dinmedia.de/en/standard/ieee-802-11bf/396264893)
- Publication slipped about 6 months from the projected March 2025 — [Du et al. overview](https://arxiv.org/abs/2310.17661)
- [V] F. Restuccia, "IEEE 802.11bf: Toward Ubiquitous Wi-Fi Sensing," arXiv 2103.14918 (Mar 2021). It reports TGbf work and a research roadmap — [arXiv](https://arxiv.org/abs/2103.14918)
- [V] R. Du, H. Hua, H. Xie, X. Song, Z. Lyu, M. Hu, Narengerile, Y. Xin, S. McCann, M. Montemurro, T. X. Han, J. Xu, "An Overview on IEEE 802.11bf: WLAN Sensing," IEEE Communications Surveys & Tutorials, 2024 (online 3 Jun 2024), DOI 10.1109/COMST.2024.3408899; arXiv 2207.04859 / 2310.17661 — [arXiv](https://arxiv.org/pdf/2310.17661); [OpenAlex](https://api.openalex.org/works/doi:10.1109%2FCOMST.2024.3408899). (Lead author Rui Du, plus Jie Xu and Tony Xiao Han, confirmed. The full middle author list is background knowledge. Check it.)

### Inferences
- Implications: (a) sensing gets standardized scheduling, which addresses the irregular-sampling problem; (b) CSI-like reports can be exposed through a standard interface rather than firmware hacks; (c) privacy and security of sensing become a standards-level concern. For the group, 802.11bf is "future work / context". Their ESP32-C6 setup relies on vendor-specific CSI callbacks, which is exactly the non-standard situation 11bf is meant to end.

### Gaps
- No verified information on shipping 802.11bf-certified chipsets or Wi-Fi Alliance certification programs as of late 2026.

---

## 9. Privacy and security

### Takeaway
The same ability that enables sensing also enables surveillance. A passive eavesdropper can infer motion or presence through walls from ordinary traffic. Countermeasures (IRS-based channel obfuscation, signal randomization) are early research. Consent and governance are not solved, and a home-deployable ESP32 sensor raises these issues directly.

### Cited Findings
- [V] P. Staat, S. Mulzer, S. Roth, V. Moonsamy, M. Heinrichs, R. Kronberger, A. Sezgin, C. Paar, "IRShield: A Countermeasure Against Adversarial Physical-Layer Wireless Sensing," IEEE S&P 2022, DOI 10.1109/SP46214.2022.00097, arXiv 2112.01967. Passive eavesdroppers can infer human motion from channel observations "using simple statistical methods". IRShield uses intelligent reflecting surfaces to obfuscate the channel and cut detection to ≤5% against a state-of-the-art attack — [arXiv](https://ar5iv.labs.arxiv.org/html/2112.01967); [AMiner](https://www.aminer.org/pub/61ad7f505244ab9dcbc6d051/irshield-a-countermeasure-against-adversarial-physical-layer-wireless-sensing). (Author names beyond Mulzer come from background knowledge. Check them on arXiv.)
- Tan et al. present WiFi sensing as *reducing* privacy risk relative to cameras — [Tan et al. 2022](https://par.nsf.gov/biblio/10473796-commodity-wifi-sensing-ten-years-status-challenges-opportunities). This contrasts with IRShield's threat model, and the tension is worth noting in the report.
- Open follow-ups noted for IRShield: optimizing IRS configurations and handling stronger attackers — [TechXplore](https://techxplore.com/news/2022-05-countermeasure-unwanted-wireless-surveillance.pdf)

### Inferences
- "Privacy-preserving vs. cameras" holds only against *visual* identification. CSI can still reveal presence, routine and potentially identity (gait). The group should discuss data minimization (on-device feature extraction, no raw CSI over MQTT, TLS on the broker).

### Gaps
- Did not verify identity-inference attacks (e.g., WiWho, gait ID) or 802.11bf's security/privacy clauses this session.

---

## 10. Cost and hardware accessibility

### Takeaway
A decade of CSI research rests on the Intel 5300 and the Linux 802.11n CSI Tool (custom firmware, 5300-only). That card was discontinued around 2010 and is end-of-life. Alternatives (Atheros CSI Tool, Nexmon on Broadcom/Raspberry Pi, PicoScenes on Intel AX200/AX210, ESP32) differ in antennas, bandwidth, phase quality and formats, which fragments results. ESP32 lowers cost a lot but trades away antennas, bandwidth and phase.

### Cited Findings
- The Linux 802.11n CSI Tool uses custom firmware that works only on the Intel IWL5300. Intel's PCN110203 set the discontinuation cutoff, and Intel lists the 5300 as end-of-life with no further security updates — [CSI Tool](https://dhalperi.github.io/linux-80211n-csitool); [FAQ](https://dhalperi.github.io/linux-80211n-csitool/faq.html); [Intel PCN110203](https://cdrdv2-public.intel.com/806906/PCN110203-00.pdf); [Intel support](https://www.intel.com/content/www/us/en/support/articles/000006507.html). (The "2010 cutoff" comes from a search summary of the PCN. Check the date in the PDF.)
- Alternatives: Atheros CSI Tool (e.g., AR9580); Nexmon CSI (Broadcom, e.g., BCM43455c0 on Raspberry Pi); PicoScenes (Intel AX200/AX210); ESP32-CSI-Tool — [CSIKit hardware list](https://github.com/rack570/CSIKit); [nexmon_csi](https://relatedrepos.com/gh/nexmonster/nexmon_csi); [Intel community thread](https://community.intel.com/t5/Wireless/Can-I-collect-CSI-data-on-Desktop-using-Intel-5300-NIC/m-p/560943)
- [V] S. M. Hernandez, E. Bulut, "Lightweight and Standalone IoT Based WiFi Sensing for Active Repositioning and Mobility," IEEE WoWMoM 2020, Cork, pp. 277–286. This is the standard citation for the ESP32 CSI tool — [PDF](https://ebulutvcu.github.io/wowmom20-WiFiSensing.pdf); [researchr](https://researchr.org/publication/HernandezB20-2)
- Espressif offers official sensing components (esp_wifi_sensing, esp_csi_gain_ctrl) — [esp_wifi_sensing](https://components.espressif.com/components/espressif/esp_wifi_sensing); [esp_csi_gain_ctrl](https://components.espressif.com/components/espressif/esp_csi_gain_ctrl/versions/0.1.5/readme)
- Tan et al. argue commodity hardware enables wide deployment by reusing existing home WiFi devices "without incurring an additional cost" — [Tan et al. 2022](https://par.nsf.gov/biblio/10473796-commodity-wifi-sensing-ten-years-status-challenges-opportunities)

### Inferences
- Positioning statement for the report: the project sits at the cheapest, most constrained end of the hardware range (single antenna, 20 MHz, amplitude-only). That is where reproducibility and deployability matter most and where the literature is thinnest. It is a defensible gap, not just a limitation.

### Gaps
- No verified comparison table across platforms (cost/bandwidth/antennas/phase quality) was found.
- ESP32-C6-specific CSI characteristics (subcarrier count under HE vs. legacy frames, AGC exposure in the API) were not verified this session. Check against Espressif's ESP-IDF docs.
