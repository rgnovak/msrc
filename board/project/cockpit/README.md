# Cockpit extension (Core2350B)

Quick reference for everything in this folder that's meant to be
tweaked before a rebuild, without having to read the .c files first -
useful after a break, or if the build environment ever has to be set
up from scratch again.

## Building

This board is the RP2350B variant (48 GPIOs), and the pico-sdk ships
its exact definition as `waveshare_core2350b`. Configure with:

```
cmake -S board -B build -DPICO_BOARD=waveshare_core2350b \
      -DPICO_PLATFORM=rp2350-arm-s -DPICO_SDK_FETCH_FROM_GIT=on
cmake --build build -j$(nproc)
```

Don't use `-DPICO_BOARD=pico2` - that's the generic 30-GPIO RP2350A
board definition. It still compiles, but caps addressable GPIOs at
29 (exactly the range MSRC's own pins already fill) and blinks the
wrong onboard LED pin. Switching the board needs a fresh configure
(delete `build/`, or point `-B` at a new folder), not just a rebuild.

## Pins and timing: `cockpit_pins.h`

Every GPIO the displays, the pilot-head servo and the IMU use, plus
the bootscreen delay, are defined once in `cockpit_pins.h`. Change a
value there and rebuild - nothing else in this folder hardcodes a pin
number or that delay. All of them sit at GPIO32 and above on purpose,
clear of `constants.h`'s MSRC pins (0-15, 26-29) and the onboard
LED/WS2812 pins (16, 39), so this chip can run full MSRC telemetry
(vario, GPS, ESC, fuel meter, ...) alongside the cockpit without a
pin collision. GPIO44-46 are left free for the future Cockpit-Audio
(ESP32->RP2350) pins; GPIO47 is the onboard PSRAM chip-select - leave
that one alone.

## Swapping the moving map: `map_bitmap.h`

The map background is just a 128x128 RGB565 pixel array (`mapBitmap`)
in `map_bitmap.h`, `#include`d by `map.c` - exactly how the original
Arduino sketch kept its map image in its own header
(`map_128x128_final.h`), separate from the rendering code. To use a
different map, drop a new `map_bitmap.h` with the same array name and
size in its place and rebuild - no other code needs to change. The
screenshot -> header conversion workflow itself isn't written up yet
(deferred until the rest of the port is running on real hardware).
