# Attribution and modifications

The combined source distribution uses GPLv3. Individual file notices and
component license terms are retained.

| Component | Origin and notices | Changes |
|---|---|---|
| Handy | [libretro-handy](https://github.com/libretro/libretro-handy) and [Retro-Go](https://github.com/ducalex/retro-go), imported Handy component snapshot. K. Wilkins' zlib-style notice is preserved in `deps/handy/license.txt`. The EEPROM and Wookie boot-decryption source notices remain in their files. | Altered Handy source. `system.cpp` and `lynxdec.cpp` implement software boot with guarded scan and fixed-width boot helpers. `mikie.cpp` retains timer and audio behavior with batched equivalent paths. CPU dispatch, sprite processing and EEPROM export are adapted for RP2350. |
| HDMI | [pico_shared](https://github.com/fhoedemakers/pico_shared) family, GPLv3 platform distribution | `deps/hdmi/` is an adapted driver snapshot with bounded audio queue, double buffering and an equivalent packet encoder. The public GPIO mapping is in CMake. |
| SD and FatFs | [elehobica/pico_fatfs](https://github.com/elehobica/pico_fatfs), BSD-2-Clause, and FatFs per-file notices | `deps/fatfs/` retains the adapted SD driver with a guarded ROM-read clock scope and restoration. |
| Shared configuration | [pico_shared](https://github.com/fhoedemakers/pico_shared) at `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0`, GPL-3.0-only | Selected shared headers. |
| Port integration | BSD-3-Clause where marked, `LICENSE.BSD-3-Clause` | `firmware/` contains the cartridge runtime, CRC composition, input, original PCM transport, timing and EEPROM save integration. |
| Build dependencies | Public revisions in `dependencies.lock.json` | Pico SDK, TinyUSB and Pico-PIO-USB are fetched separately. |

The imported Handy snapshot has no Git metadata. The pre-adaptation
file hashes recorded at import are retained in
`deps/handy/SNAPSHOT_SHA256.json`. They identify the source snapshot
and are distinct from the modified files in this repository.

The public build contains software boot code and accepts external game
files. Game images, save files and Atari BIOS dumps are excluded.
