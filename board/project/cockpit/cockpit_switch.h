#ifndef COCKPIT_SWITCH_H
#define COCKPIT_SWITCH_H

#include <stdbool.h>

/* SRXL2 equivalent of RC-Cockpit's pwm_input.cpp. The original read
 * a 3-position switch as a raw PWM pulse (1000/1500/2000us) on a
 * dedicated GPIO; MSRC already decodes this same switch's value out
 * of the SRXL2 stream as a channel (see srxl2_channels.h), so this
 * module classifies that channel's value into the same three zones
 * with the same hysteresis thresholds, just reading a decoded
 * channel instead of measuring a pulse. Which channel is read is
 * set once in cockpit_channels.h. */

typedef enum {
    COCKPIT_SWITCH_LOW,
    COCKPIT_SWITCH_CENTER,
    COCKPIT_SWITCH_HIGH,
} cockpit_switch_zone_t;

/* Call once per cockpit_task loop iteration. */
void cockpitSwitchUpdate(void);

cockpit_switch_zone_t cockpitSwitchGetZone(void);

/* false when the switch channel hasn't shown up in a recent SRXL2
 * frame (receiver unbound, transmitter off, link lost) - mirrors
 * pwm_input's signal-timeout fallback, which the caller should treat
 * the same way: fail safe (displays + head movement both on). */
bool cockpitSwitchSignalPresent(void);

#endif /* COCKPIT_SWITCH_H */
