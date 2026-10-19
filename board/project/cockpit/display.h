#ifndef COCKPIT_DISPLAY_H
#define COCKPIT_DISPLAY_H

/* New module - this didn't exist in the Arduino sketch, where all of
 * this lived directly in the .ino's setup()/loop(). It's split out
 * here because it's the only part of the port that's genuinely new
 * code (a bare pico-sdk SPI driver) rather than a straight port: pfd.c
 * / map.c / bootscreen.c are pure software rendering and only ever
 * touch backFB, never the hardware.
 *
 * Confirmed against a real, flashed RP2350 build of this same
 * cockpit code (a 4-display Waveshare RP2350 Tiny variant): SCK/MOSI
 * on GPIO6/7 is SPI0 on RP2350, the ST7735 init sequence below, and
 * the panelColor()/raw-framebuffer-push byte order in pfd.h/pfd.c all
 * come from that working build. Pin numbers here are for the
 * Core2350B 2-display board (same SPI/DC/RST/backlight pins, just
 * CS1/CS2 instead of CS1..CS4) - see cockpit_pins.h, the single place
 * that now defines them. */

#include <stdbool.h>
#include <stdint.h>

#include "cockpit_pins.h"

/* Brings up SPI0 + both ST7735 panels, clears them to black, turns
 * the backlights on, and allocates the two framebuffers (frontFB/
 * backFB, the latter exposed to the renderer through pfd.h). Call
 * once at startup, before any drawing or push call. */
void cockpitDisplayBegin(void);

void cockpitDisplaySetBacklight(bool on);
bool cockpitDisplayGetBacklight(void);

/* Swaps backFB into frontFB and pushes the result to one panel
 * (COCKPIT_PIN_CS1 or COCKPIT_PIN_CS2). Call once per rendered
 * instrument: draw into backFB via pfd.c/map.c's functions, then
 * call this with that instrument's CS pin. */
void cockpitDisplaySwapAndPush(uint8_t cs);

/* Mirrors the original sketch's one-off bootscreen choreography:
 * swap once, push the same framebuffer to both panels (no swap
 * between the two pushes, since the splash is identical on both),
 * then swap back so the caller gets a fresh backFB to render the
 * first real frame into. Draw the splash into backFB (e.g. with
 * showGarminBootscreen()) before calling this. */
void cockpitDisplayPushBootscreen(void);

#endif /* COCKPIT_DISPLAY_H */
