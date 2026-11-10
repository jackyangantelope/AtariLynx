# Release validation

| Check | Result |
|---|---|
| 0.1.39 ON physical baseline, November 6, 2026 | Ynxa passed user acceptance for frame rate, continuous audio and restored progress on re-entry. The analyzed gameplay interval recorded about 59.80 emulated FPS and 43.96 frame submissions per second. Four re-entries averaged 0.782283 seconds. Cold-start time was unavailable in the capture. |
| Public Fruit Jam build | Independent clean clone compiled with the pinned Pico SDK and Arm GNU Toolchain 14.2.Rel1. Flash 279,588 B and main RAM 434,920 B (82.95%) at link time. |
| Cartridge CRC | 4,238 native cases passed with ASan/UBSan, including an external Ynxa image. The CRC composition and table path matched the scalar oracle. |
| Public Fruit Jam physical test | Pending. HDMI, SD and USB use Fruit Jam wiring. Physical audio/video timing, gameplay and save restoration remain unverified on this configuration. |

Both public builds produced UF2 SHA-256
`c2993a6aec8b139cf5ab7640a904e7af7491b66109601532d3e60d209525c3a3`.
The source audit passed its blocking checks. Only this validation record
was added after the clean-clone build.
