# Literature Review: Wi-Fi Sensing Using Channel State Information

**RIDE Fall 2026 · Group 6** — Yusuf Shaikh, Thomas Alcorn, Karl Wara, Vihaan Vuppala — November 11, 2026

## 1. Introduction

Wi-Fi channel state information (CSI) describes how a room changes a radio signal on each of the narrow frequencies (subcarriers) that make up a Wi-Fi channel. Because a moving body changes the signal paths between two radios, CSI can detect presence, recognize activities and locate people without cameras or wearables. This review surveys the measurement tools, landmark sensing systems, work on low-cost ESP32 hardware, and multi-link localization. It then identifies the gaps that our system can address.

Our system uses ESP32-C6 boards: one antenna, 2.4 GHz, 20 MHz 802.11n frames with 56 usable subcarriers, and amplitude only. One transmitter broadcasts 100 frames per second. Any number of receivers capture CSI and report it over MQTT to a hub, and the boards are not clock-synchronized. Receivers are added by configuration alone. The project's modules are a measurement platform, presence and motion detection (empty, occupied and still, occupied and moving), and three-zone localization.

## 2. Background

An 802.11n channel is divided into subcarriers, and each frame starts with known training symbols, so the receiver can estimate a complex gain for every subcarrier. That gain is the sum of all propagation paths: direct, reflected off walls, and reflected off people. When someone moves, the paths they touch change length and the subcarrier pattern changes. Because each subcarrier has a slightly different wavelength, CSI carries far more information than a single received signal strength (RSSI) value [5].

Two models explain most sensing work. The **Fresnel-zone model** treats the region between transmitter and receiver as nested ellipses: a person crossing them makes the amplitude rise and fall, and some positions are blind spots where breathing cannot be seen [11]. The **CSI-speed model** (CARM) links how fast CSI power fluctuates to how fast body parts move. It uses power rather than phase because power is unaffected by carrier frequency offset (CFO) [10].

Phase matters for the hardware choice. Commodity radios add random phase offsets to every frame. Multi-antenna systems cancel these by dividing the CSI of two antennas that share an oscillator [12]. A single-antenna ESP32 cannot do this, and separate boards have separate clocks, so phase-based methods are out of reach without shared-clock hardware [9]. Our system therefore uses amplitude only.

## 3. CSI measurement tools

Until about 2019 nearly all CSI research used laptop-hosted network cards: 92.5% of the 494 papers listed on the tools' websites used the Intel 5300 [5], which is now end-of-life.

| Tool | Hardware | Antennas | Subcarriers | Max rate | Cost |
|---|---|---|---|---|---|
| Linux 802.11n CSI Tool [1] | Intel 5300 + laptop | 3 | 30 groups | ~1000/s | ~$10 card + PC |
| Atheros CSI Tool [2] | Qualcomm Atheros + PC | 3 | 56 | ~1000/s | ~$10 card + PC |
| Nexmon CSI [3] | Broadcom (phones, Raspberry Pi) | 1+ | 64–256 | traffic-dependent | >$100 |
| PicoScenes [4] | Intel AX200/AX210, SDRs | multiple | up to 160 MHz | ~1000/s | card + PC |
| ESP32 CSI Tool / esp-csi [5], [7] | ESP32 family, standalone | 1 | 64 (56 usable) | ~650/s | <$10 |

The ESP32's disadvantages are its single antenna, 8-bit resolution and unusable phase, not its frame rate. Its advantages are cost, standalone operation and easy deployment in numbers. On the ESP32-C6 specifically, an open Espressif issue confirms that 20 MHz frames return 64 subcarrier slots with 56 effective, and that invalid CSI can reach the application unless filtered [8].

## 4. Landmark sensing systems

Research divides into **model-based** systems, which derive features from propagation physics, and **learning-based** systems, which match CSI against labelled examples. Model-based systems generalize better between rooms but usually need phase or several antennas. Learning-based systems work on any data but tend to be tied to the room they were trained in. Table 2 summarizes key systems and whether their method could run on single-antenna, amplitude-only hardware.

| System | Year | Task | Hardware | Reported result | Amplitude-only? |
|---|---|---|---|---|---|
| FIMD [13] | 2012 | Motion detection | Intel NIC | Better than RSSI | Yes |
| Pilot [14] | 2013 | Presence + localization | Multiple links | Large gain over RSS methods | Yes |
| E-eyes [15] | 2014 | Daily activities | Intel 5300 | >96% detection, <1% false positives | Yes |
| CARM [10] | 2015 | 8 activities | Intel 5300 | >96%; >80% new room and person | Yes (needs high rate) |
| SpotFi [16] | 2015 | Localization (device) | 3 antennas | 40 cm median error | No |
| LiFS [17] | 2016 | Localization (device-free) | Multiple links | 0.5 m (LOS), 1.1 m (NLOS) median | Yes, in principle |
| EI [19] | 2018 | Activity, any room | Hardware-agnostic | Environment-independent learning | Yes |
| FarSense [12] | 2019 | Breathing at range | 2 antennas | ~8 m range | No |
| Widar 3.0 [18] | 2019 | Gestures, any room | Multiple receivers | 92.7% in-domain; 82.6–92.4% cross-domain | No |

Two patterns stand out. First, the accuracy ceiling since 2016 has been set by phase-based, multi-antenna systems that ESP32 hardware cannot reproduce; the methods that transfer are the amplitude-statistics ones (FIMD, E-eyes, CARM, LiFS). Second, results above 90% in the training room typically drop by 10–15 points in a new room [10], [18].

## 5. ESP32-based work

Hernandez and Bulut's ESP32 CSI Tool established the ESP32 as a standalone CSI sensor and found that moving the receiver changed activity-recognition accuracy by up to about 29%, so **receiver placement is a first-order variable** [5]. Their later survey measured signal-processing cost on board the ESP32 [6]. The strongest peer-reviewed ESP32 result is through-wall activity recognition with ESP32-S3 boards and directional antennas: 92.0% accuracy without line of sight over 18 m [20]. Public ESP32 datasets now exist, for example a four-board activity dataset [35]. Macedo et al. built a four-ESP32 localization testbed and removed one receiver at a time to measure each one's contribution [21].

The closest work to ours is recent and not yet peer-reviewed. One preprint uses three ESP32 links for sub-metre fingerprint localization [22]. Another uses one ESP32 link per zone across four zones for occupancy, with an F1 score of 0.96 for empty versus occupied [23]. A third ran a nine-node, 72-link ESP32-C3 mesh in a home for 12 days and found that **one well-placed link beat the full mesh**, though both scored near chance, and that placement mattered 2.7 times more than the choice of classifier [24]. At the other end of the design space, ESPARGOS makes ESP32 phase usable by sharing one clock across eight chips on a custom board [9].

To our knowledge there is no peer-reviewed ESP32-C6 CSI sensing study, and no published measurement of a one-transmitter, many-receiver ESP32 network, although Espressif recommends that topology without publishing results [7].

## 6. Multi-link device-free localization

Device-free localization began with Youssef et al. [25]. Wilson and Patwari's **radio tomographic imaging (RTI)** modelled each link's signal-strength loss as coming from an ellipse around the link and reconstructed an image of the room from 28 low-cost nodes [26]. Variance-based RTI tracked people through walls using the variance of signal strength [27]. That is the direct ancestor of our motion score, and it shares the same weakness: a still person creates little variance. Zhao et al. solved this for RSS by comparing the current signal distribution against an empty-room baseline (a kernel distance), which located still as well as moving people [28].

RTI networks are meshes in which every pair of nodes forms a link, so the number of links grows quadratically and is hard to vary cleanly. CSI-based systems such as Pilot and LiFS added subcarrier detail, but on multi-antenna laptop cards [14], [17]. Low-cost multi-receiver systems align frames across receivers with a sequence number rather than synchronized clocks [9], and no study we found measures how much timing misalignment amplitude features tolerate.

## 7. Open challenges

- **Generalization across environments.** Surveys identify domain shift, from changes in room, person or device, as the field's central unsolved problem [29], [30], [34]. The strongest fix (Widar 3.0) needs phase [18].
- **Detecting still people.** Motion is easy; a seated or standing person changes the channel very little, and breathing detection depends on centimetre-scale position [11]. The published fixes need two antennas [12] or have been demonstrated only with RSS [28].
- **Receiver hardware effects.** Automatic gain control is the dominant source of differences between receivers seeing the same signal, and correcting for it recovers up to 75% of lost cross-device accuracy [31]. In a network of nominally identical boards, board-to-board differences may matter as much as room changes.
- **Irregular sampling and shared airtime.** CSI exists only when a frame arrives. Sensing traffic competes with normal Wi-Fi traffic, which the new IEEE 802.11bf sensing standard addresses at the protocol level only [32].
- **Benchmarks.** Public benchmarks such as SenseFi use multi-antenna card data that does not transfer to 56-subcarrier ESP32 amplitude [33].
- **Privacy.** Passive CSI can reveal presence and routines to an eavesdropper [36].

## 8. Gaps and proposed contributions

The gaps our system can address are **measurement gaps about deployment**: how many receivers are needed, which signal to use, and how to detect stillness. They are not gaps in algorithms. Our configuration-only design makes these measurements unusually cheap, because each added receiver contributes exactly one new link and costs only a settings change.

1. **Accuracy versus number of receivers (primary).** Measure presence and zone accuracy for every subset of up to six receivers around one transmitter, including leave-one-receiver-out robustness. RTI argues that more nodes help [26], while the ESP32 mesh study found the opposite [24]. No study we found reports this curve for a one-transmitter star of low-cost CSI receivers.
2. **CSI versus RSSI from the same frames (primary).** Every CSI report also carries RSSI, so the two can be compared with identical hardware, placement and labels. Earlier CSI-beats-RSS claims came from different hardware [13], [14], and the ESP32 preprints compare only against literature [22].
3. **Detecting a still person with several links (secondary).** Apply the empty-room distribution distance from RTI [28] to per-subcarrier CSI amplitude, and measure how many receivers stillness detection needs. This targets our motion score's known weakness.
4. **Timing tolerance (secondary).** Align receivers by the transmitter's frame counter, then add artificial offsets to measure how much misalignment the features tolerate.

All four can be answered from **one labelled, multi-session recording campaign** with six receivers, so they fit the project timeline. These novelty claims rest on the sources found to date. Two of the closest preprints [22], [23] could not be read in full, so they will be checked before the final report.

## 9. Conclusion

Fifteen years of research show that Wi-Fi CSI reliably detects motion and can recognize activities and locate people. The best results, however, depend on multi-antenna hardware and on phase, and they degrade in new rooms. Low-cost ESP32 sensing is growing fast but has not been characterized for the configuration we built: one broadcaster heard by many cheap, unsynchronized receivers. Our contribution is to measure what each additional receiver actually adds, whether CSI is worth its cost over RSSI, and how several receivers can detect a person who is not moving.

## References

[1] D. Halperin, W. Hu, A. Sheth, and D. Wetherall, "Tool release: Gathering 802.11n traces with channel state information," *ACM SIGCOMM Comput. Commun. Rev.*, vol. 41, no. 1, 2011.

[2] Y. Xie, Z. Li, and M. Li, "Precise power delay profiling with commodity WiFi," in *Proc. ACM MobiCom*, 2015, pp. 53–64.

[3] F. Gringoli, M. Schulz, J. Link, and M. Hollick, "Free your CSI: A channel state information extraction platform for modern Wi-Fi chipsets," in *Proc. ACM WiNTECH*, 2019.

[4] Z. Jiang et al., "Eliminating the barriers: Demystifying Wi-Fi baseband design and introducing the PicoScenes Wi-Fi sensing platform," *IEEE Internet Things J.*, vol. 9, no. 6, pp. 4476–4496, 2022.

[5] S. M. Hernandez and E. Bulut, "Lightweight and standalone IoT based WiFi sensing for active repositioning and mobility," in *Proc. IEEE WoWMoM*, 2020, pp. 277–286.

[6] S. M. Hernandez and E. Bulut, "WiFi sensing on the edge: Signal processing techniques and challenges for real-world systems," *IEEE Commun. Surveys Tuts.*, vol. 25, no. 1, pp. 46–76, 2023.

[7] Espressif Systems, "ESP-CSI," GitHub repository. https://github.com/espressif/esp-csi

[8] Espressif Systems, "ESP32-C6 not reporting L-LTF data in WiFi CSI callback and wrong subcarrier order for HT-LTF," esp-idf issue #14271. https://github.com/espressif/esp-idf/issues/14271

[9] F. Euchner, T. Schneider, M. Gauger, and S. ten Brink, "ESPARGOS: An ultra low-cost, realtime-capable multi-antenna WiFi channel sounder," arXiv:2502.09405 (preprint), 2025.

[10] W. Wang, A. X. Liu, M. Shahzad, K. Ling, and S. Lu, "Understanding and modeling of WiFi signal based human activity recognition," in *Proc. ACM MobiCom*, 2015.

[11] H. Wang, D. Zhang et al., "Human respiration detection with commodity WiFi devices: Do user location and body orientation matter?" in *Proc. ACM UbiComp*, 2016, pp. 25–36.

[12] Y. Zeng, D. Wu, J. Xiong, E. Yi, R. Gao, and D. Zhang, "FarSense: Pushing the range limit of WiFi-based respiration sensing with CSI ratio of two antennas," *Proc. ACM IMWUT*, vol. 3, no. 3, 2019.

[13] J. Xiao, K. Wu, Y. Yi, L. Wang, and L. M. Ni, "FIMD: Fine-grained device-free motion detection," in *Proc. IEEE ICPADS*, 2012, pp. 229–235.

[14] J. Xiao, K. Wu, Y. Yi, L. Wang, and L. M. Ni, "Pilot: Passive device-free indoor localization using channel state information," in *Proc. IEEE ICDCS*, 2013, pp. 236–245.

[15] Y. Wang, J. Liu, Y. Chen, M. Gruteser, J. Yang, and H. Liu, "E-eyes: Device-free location-oriented activity identification using fine-grained WiFi signatures," in *Proc. ACM MobiCom*, 2014.

[16] M. Kotaru, K. Joshi, D. Bharadia, and S. Katti, "SpotFi: Decimeter level localization using WiFi," in *Proc. ACM SIGCOMM*, 2015.

[17] J. Wang et al., "LiFS: Low human-effort, device-free localization with fine-grained subcarrier information," in *Proc. ACM MobiCom*, 2016, pp. 243–256.

[18] Y. Zheng et al., "Zero-effort cross-domain gesture recognition with Wi-Fi," in *Proc. ACM MobiSys*, 2019.

[19] W. Jiang et al., "Towards environment independent device free human activity recognition," in *Proc. ACM MobiCom*, 2018, pp. 289–304.

[20] J. Strohmayer and M. Kampel, "Directional antenna systems for long-range through-wall human activity recognition," in *Proc. IEEE ICIP*, 2024. arXiv:2401.01388.

[21] D. Macedo, M. Loureiro, Ó. G. Martins, J. C. Sousa, D. Belo, and M. Gomes, "From CSI to coordinates: An IoT-driven testbed for individual indoor localization," *Future Internet*, vol. 17, no. 9, p. 395, 2025.

[22] "Device-free indoor localization with ESP32 Wi-Fi CSI fingerprints: Grid-based regression, feature modeling, and multi-link experiments," Preprints.org 202601.2378 (preprint), 2026.

[23] Chaudhari, Pise, Fukate, and Gawande, "Room-level occupancy estimation via Wi-Fi channel state information on ESP32 nodes," Preprints.org 202601.2295 (preprint), 2026.

[24] K. Khamaisi and B. Rodrigues, "Less is more: The dilution effect in multi-link wireless sensing," arXiv:2602.10823 (preprint), 2026.

[25] M. Youssef, M. Mah, and A. Agrawala, "Challenges: Device-free passive localization for wireless environments," in *Proc. ACM MobiCom*, 2007, pp. 222–229.

[26] J. Wilson and N. Patwari, "Radio tomographic imaging with wireless networks," *IEEE Trans. Mobile Comput.*, vol. 9, no. 5, pp. 621–632, 2010.

[27] J. Wilson and N. Patwari, "See-through walls: Motion tracking using variance-based radio tomography networks," *IEEE Trans. Mobile Comput.*, vol. 10, no. 5, pp. 612–621, 2011.

[28] Y. Zhao, N. Patwari, J. M. Phillips, and S. Venkatasubramanian, "Radio tomographic imaging and tracking of stationary and moving people via kernel distance," in *Proc. ACM/IEEE IPSN*, 2013, pp. 229–240.

[29] Y. Ma, G. Zhou, and S. Wang, "WiFi sensing with channel state information: A survey," *ACM Comput. Surveys*, vol. 52, no. 3, 2019.

[30] C. Chen, G. Zhou, and Y. Lin, "Cross-domain WiFi sensing with channel state information: A survey," *ACM Comput. Surveys*, vol. 55, no. 11, 2023.

[31] Portner, Gringoli, Hollick, and Asadi, "Same signal, different story: Demystifying receiver effects in Wi-Fi channel state information," arXiv:2605.26836, 2026.

[32] *IEEE Std 802.11bf-2025*, Enhancements for Wireless LAN Sensing, 2025.

[33] J. Yang, X. Chen et al., "SenseFi: A library and benchmark on deep-learning-empowered WiFi human sensing," *Patterns*, vol. 4, no. 3, 100703, 2023.

[34] S. Tan, Y. Ren, J. Yang, and Y. Chen, "Commodity WiFi sensing in ten years: Status, challenges, and opportunities," *IEEE Internet Things J.*, vol. 9, no. 18, pp. 17832–17843, 2022.

[35] W. E. Wong, A. H. Wong, W. Q. Peh, and C. K. Tan, "A high-dimensional, multi-transceiver channel state information dataset for enhanced human activity recognition," *Data in Brief*, vol. 55, 2024.

[36] P. Staat et al., "IRShield: A countermeasure against adversarial physical-layer wireless sensing," in *Proc. IEEE S&P*, 2022.
