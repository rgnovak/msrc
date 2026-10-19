#ifndef COCKPIT_PINS_H
#define COCKPIT_PINS_H

/* Single place for every GPIO the cockpit extension (both displays,
 * the pilot-head gaze servo, the IMU) uses on the Core2350B, plus a
 * couple of timing values worth tweaking without digging through the
 * .c files. Change a value here and rebuild - nothing else under
 * cockpit/ hardcodes a pin number or these timings anymore. This
 * mirrors board/project/constants.h, which does the same job for
 * MSRC's own receiver/sensor pins.
 *
 * REQUIRES THE CORRECT BOARD: this chip is the RP2350B variant (48
 * GPIOs, not 30), and the pico-sdk ships its exact board definition
 * as "waveshare_core2350b". Configure with:
 *   cmake -S board -B build -DPICO_BOARD=waveshare_core2350b \
 *         -DPICO_PLATFORM=rp2350-arm-s -DPICO_SDK_FETCH_FROM_GIT=on
 * NOT -DPICO_BOARD=pico2 (that's the generic 30-GPIO RP2350A board
 * definition - it still compiles, but caps addressable GPIOs at 29,
 * which is exactly the range constants.h's MSRC pins already fill,
 * and it blinks the wrong onboard LED pin). Switching board needs a
 * fresh cmake configure (delete build/ or point -B at a new folder),
 * not just a rebuild.
 *
 * PIN BUDGET: constants.h's MSRC pins (0-15, 26-29), the onboard LED
 * (GPIO39) and the WS2812 pin hardcoded in led.c (GPIO16) all live
 * below GPIO32. Every cockpit pin below is >=32 on purpose, so this
 * chip can run full MSRC telemetry (vario, GPS, ESC, fuel meter, ...)
 * at the same time without any pin collision. GPIO47 is the onboard
 * PSRAM chip-select (WAVESHARE_CORE2350B_PSRAM_CS_PIN) - leave it
 * alone too. GPIO44-46 are free and reserved for the future
 * Cockpit-Audio (ESP32->RP2350) pins.
 *
 * One pre-existing MSRC-core quirk this board switch exposes, not
 * fixed here: board/project/sensor/{voltage,current,ntc,airspeed}.c
 * hardcode "+ 26" to turn an ADC channel into a GPIO number. That's
 * only correct on the 30-GPIO (RP2350A) ADC layout; on this RP2350B
 * board the ADC pins are GPIO40-47, not 26-29. Harmless unless one of
 * those four sensors gets enabled on this chip - flag it if/when
 * that happens, fix belongs on rp2350-port first per our branch
 * split. */

/* ---- Displays (2x ST7735, shared SPI0) ---- */
#define COCKPIT_PIN_IMU_SDA 32 /* blau - I2C0 SDA on this pin pair */
#define COCKPIT_PIN_IMU_SCL 33 /* gelb - I2C0 SCL on this pin pair */
#define COCKPIT_PIN_SPI_SCK 34   // SPI0 SCK on this pin pair
#define COCKPIT_PIN_SPI_MOSI 35  // SPI0 TX/MOSI on this pin pair
#define COCKPIT_PIN_CS1 36  // Display 1 - PFD
#define COCKPIT_PIN_CS2 37  // Display 2 - Moving Map
#define COCKPIT_PIN_DC 38
#define COCKPIT_PIN_RST 40
#define COCKPIT_PIN_BL1 41
#define COCKPIT_PIN_BL2 42

/* ---- Pilot-head gaze servo ---- */
#define COCKPIT_PIN_PILOT_SERVO 43

/* ---- Timing ---- */
/* How long the one-time Garmin bootscreen stays up (ms) after the
 * displays are first switched on, before the first real PFD/map
 * frame is drawn. */
#define COCKPIT_BOOTSCREEN_MS 4000

#endif /* COCKPIT_PINS_H */
