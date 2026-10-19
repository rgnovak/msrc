#include "cockpit_switch.h"

#include "cockpit_channels.h"
#include "pico/time.h"
#include "srxl2_channels.h"

/* Same zone thresholds as the original pwm_input.cpp, in the
 * microsecond-equivalent units srxl2_channel_to_us() returns
 * (~988..2012us, center 1500us - the same convention a standard RC
 * PWM pulse uses, which is exactly why pwm_input's thresholds carry
 * over unchanged). */
static const float LOW_ENTER = 1300.0f;   /* < 1300us  -> LOW */
static const float LOW_EXIT = 1350.0f;    /* > 1350us  -> back toward CENTER */
static const float HIGH_ENTER = 1700.0f;  /* > 1700us  -> HIGH */
static const float HIGH_EXIT = 1650.0f;   /* < 1650us  -> back toward CENTER */

/* No new SRXL2 Control frame for this long -> treat the link as
 * lost and fall back to CENTER, same idea as pwm_input's 250ms PWM
 * signal timeout (there measured between pulse edges, here between
 * parsed frames). */
static const uint32_t SIGNAL_TIMEOUT_MS = 250;

static cockpit_switch_zone_t currentZone = COCKPIT_SWITCH_CENTER;
static bool signalPresent = false;
static bool haveSeenAnyFrame = false;
static uint32_t lastFrameCountSeen = 0;
static uint32_t lastFrameSeenMs = 0;

void cockpitSwitchUpdate(void) {
    srxl2_channels_t ch;
    srxl2_channels_get(&ch);

    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (ch.frame_count != lastFrameCountSeen) {
        lastFrameCountSeen = ch.frame_count;
        lastFrameSeenMs = now;
        haveSeenAnyFrame = true;
    }

    bool channelKnown = (ch.mask & (1u << (COCKPIT_CH_SWITCH - 1))) != 0;
    bool frameRecent = haveSeenAnyFrame && (now - lastFrameSeenMs) <= SIGNAL_TIMEOUT_MS;
    signalPresent = frameRecent && channelKnown;

    if (!signalPresent) {
        currentZone = COCKPIT_SWITCH_CENTER;
        return;
    }

    float us = srxl2_channel_to_us(ch.raw[COCKPIT_CH_SWITCH - 1]);

    switch (currentZone) {
        case COCKPIT_SWITCH_CENTER:
            if (us < LOW_ENTER)
                currentZone = COCKPIT_SWITCH_LOW;
            else if (us > HIGH_ENTER)
                currentZone = COCKPIT_SWITCH_HIGH;
            break;
        case COCKPIT_SWITCH_LOW:
            if (us > LOW_EXIT) currentZone = COCKPIT_SWITCH_CENTER;
            break;
        case COCKPIT_SWITCH_HIGH:
            if (us < HIGH_EXIT) currentZone = COCKPIT_SWITCH_CENTER;
            break;
    }
}

cockpit_switch_zone_t cockpitSwitchGetZone(void) { return currentZone; }

bool cockpitSwitchSignalPresent(void) { return signalPresent; }
