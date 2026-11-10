# Atari Lynx for RP2350

Atari Lynx cartridge runtime based on the tested 0.1.39 ON port of
[Handy](https://github.com/libretro/libretro-handy).
The public target is Adafruit Fruit Jam with 8 MiB PSRAM, SD storage,
HDMI video/audio and USB HID input. It runs the RP2350 at 252 MHz and
HSTX at 126 MHz. The direct NES/SNES adapter is disabled in this build.

Place your own `.lnx` cartridges under `/roms/LYNX` on the SD card.
Handy's software boot routines handle startup. EEPROM-backed games
can save progress to a `.sav` file alongside the cartridge. Use Esc
to return to the menu.

The 0.1.39 ON baseline passed scoped physical testing with Ynxa on
November 6, 2026. The user confirmed frame rate, continuous audio and
progress restoration after re-entry. The public Fruit Jam configuration
has separate board wiring and remains pending physical testing.

## Build

Install Git, CMake 3.20+, Ninja, Python 3 and Arm GNU Toolchain 14.2.Rel1.
Pico SDK 2.3.0, TinyUSB and Pico-PIO-USB revisions are pinned in
`dependencies.lock.json`.

```powershell
./scripts/fetch-dependencies.ps1 -Destination <outside-source-directory>
./scripts/build.ps1 -PicoSdkPath <pico-sdk> -PicoPioUsbPath <Pico-PIO-USB> `
  -ToolchainPath <arm-toolchain> -NinjaPath <ninja.exe>
```

For offline builds pass `-PioasmDir` and `-PicotoolDir` using local
SDK host tools. The output is `build/fruit-jam/picoAtariLynx.uf2`.
It is linked for a compatible 16 MiB flash loader at application base
`0x10080000`.

On Linux or WSL, run `bash scripts/test-host.sh /path/to/Ynxa.lnx` to
check cartridge header and payload CRC composition against an independent
scalar implementation with ASan/UBSan. Use the 2019 anniversary edition
for the archived test input. This is a CRC test of the load path.

See [THIRD_PARTY.md](THIRD_PARTY.md) for attribution and
[VALIDATION.md](VALIDATION.md) for the test scope.
