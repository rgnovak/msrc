#ifndef COCKPIT_BOOTSCREEN_H
#define COCKPIT_BOOTSCREEN_H

/* Ported from RC-Cockpit's bootscreen.h (Arduino sketch). The original
 * was header-only, with the font table and helper functions defined
 * right there and included from a single .ino translation unit; here
 * they move into bootscreen.c so the same functions don't get defined
 * once per .c file that includes this header. */

/* Draws the Garmin-style splash into backFB. Call display.c's
 * cockpitDisplayPushBootscreen() afterwards to get it onto the
 * panels. */
void showGarminBootscreen(void);

#endif /* COCKPIT_BOOTSCREEN_H */
