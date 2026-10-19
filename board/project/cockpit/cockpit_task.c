#include "cockpit_task.h"

#include "bootscreen.h"
#include "cockpit_channels.h"
#include "cockpit_pins.h"
#include "cockpit_switch.h"
#include "common.h"
#include "compat.h"
#include "display.h"
#include "imu.h"
#include "map.h"
#include "pfd.h"
#include "pico/time.h"
#include "piloten.h"
#include "srxl2_channels.h"

/* Owns both panels end to end, plus the pilot head's gaze servo and
 * the 3-position switch that used to come in over a dedicated PWM
 * wire (pwm_input.cpp in the original Arduino sketch) and now comes
 * from an SRXL2 channel instead (cockpit_switch.c), decoded by MSRC
 * itself. Toggle behavior (not level-based - only the act of
 * switching zones triggers anything) ported 1:1 from the original:
 *   - switch to HIGH zone: toggle the pilot head's movement on/off
 *   - switch to LOW zone:  toggle both displays (backlight) on/off
 *   - CENTER itself does nothing, it's just a pass-through position
 * Displays and head movement both start OFF at boot - nothing lights
 * up or moves until the switch is actually worked once - and the
 * Garmin bootscreen is shown exactly once, the first time the
 * displays get turned on, never again after that. If the SRXL2 link
 * drops (no recent frames, or this channel never seen), that's
 * treated as a safety fallback: displays and head movement both
 * forced on, same as pwm_input's old "no PWM signal" fallback.
 *
 * Pitch/roll/heading come from the ICM-20948 IMU (see imu.c). If the
 * sensor isn't found at startup, imuBegin() logs it and the loop
 * keeps running with pitch/roll/heading at 0 rather than crashing.
 * The pilot head additionally gets a rudder-channel bias (see
 * piloten.h's pilotenSetRudder()) on top of the existing roll bias.
 * Airspeed/altitude/VSI still reuse the same fixed demo values the
 * original Arduino sketch used (no ESC/airspeed sensor port yet),
 * and ownLat/ownLon (in map.c) never change yet either, so the map's
 * breadcrumb track stays put - also matching the original demo. */

static float cockpitReadRudderNorm(void) {
    srxl2_channels_t ch;
    srxl2_channels_get(&ch);

    if (!(ch.mask & (1u << (COCKPIT_CH_RUDDER - 1)))) return 0.0f;

    float us = srxl2_channel_to_us(ch.raw[COCKPIT_CH_RUDDER - 1]);
    float norm = (us - 1500.0f) / 500.0f; /* ~988..2012us -> roughly -1..+1 */
    return constrain(norm, -1.0f, 1.0f);
}

void cockpit_task(void *parameters) {
    (void)parameters;

    cockpitDisplayBegin();
    cockpitDisplaySetBacklight(false); /* dark until the switch turns it on */

    pilotenBegin(COCKPIT_PIN_PILOT_SERVO);
    pilotenSetEnabled(false); /* head movement also off at boot */

    if (!imuBegin(COCKPIT_PIN_IMU_SDA, COCKPIT_PIN_IMU_SCL)) {
        debug("\ncockpit_task: IMU nicht gefunden - Pitch/Roll/Heading bleiben auf 0");
    }

    bool displayEnabled = false;
    bool backlightIsOn = false;
    bool bootscreenShown = false;
    bool showingBootscreen = false;
    uint32_t bootscreenUntilMs = 0;

    /* Prime the zone memory with the switch's actual boot position so
     * that position itself never counts as a toggle - only a later,
     * real switch movement does. */
    cockpitSwitchUpdate();
    cockpit_switch_zone_t lastZone = cockpitSwitchGetZone();

    while (1) {
        bool displayEnabledBefore = displayEnabled;

        /* ===== SWITCH (TOGGLE, not level-based) ===== */
        cockpitSwitchUpdate();
        cockpit_switch_zone_t zone = cockpitSwitchGetZone();

        if (!cockpitSwitchSignalPresent()) {
            displayEnabled = true;
            pilotenSetEnabled(true);
            lastZone = COCKPIT_SWITCH_CENTER;
        } else if (zone != lastZone) {
            if (zone == COCKPIT_SWITCH_HIGH) {
                pilotenSetEnabled(!pilotenGetEnabled());
            } else if (zone == COCKPIT_SWITCH_LOW) {
                displayEnabled = !displayEnabled;
            }
            lastZone = zone;
        }

        /* ===== FIRST TIME THE DISPLAYS COME ON -> BOOTSCREEN ONCE ===== */
        if (displayEnabled && !displayEnabledBefore && !bootscreenShown) {
            showGarminBootscreen();
            cockpitDisplayPushBootscreen();
            cockpitDisplaySetBacklight(true);
            backlightIsOn = true;
            bootscreenShown = true;
            showingBootscreen = true;
            bootscreenUntilMs = to_ms_since_boot(get_absolute_time()) + COCKPIT_BOOTSCREEN_MS;
        }

        if (showingBootscreen && !displayEnabled) {
            showingBootscreen = false;
        }

        /* Backlight follows displayEnabled (only touch the GPIOs on
         * an actual change). */
        if (displayEnabled && !backlightIsOn) {
            cockpitDisplaySetBacklight(true);
            backlightIsOn = true;
        } else if (!displayEnabled && backlightIsOn) {
            cockpitDisplaySetBacklight(false);
            backlightIsOn = false;
        }

        /* ===== IMU / PILOT HEAD (always updated, even displays off) ===== */
        imuUpdate();

        float pitch = imuGetPitch();
        float roll = imuGetRoll();
        int ihdg = imuGetHeading();

        pilotenSetRoll(roll);
        pilotenSetRudder(cockpitReadRudderNorm());
        pilotenUpdate(); /* 1. update, before rendering - matches the
                           * original Arduino loop's "update once, then
                           * once more after each panel push" pattern,
                           * trimmed from 4 panels to this board's 2. */

        if (!displayEnabled) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (showingBootscreen) {
            if (to_ms_since_boot(get_absolute_time()) >= bootscreenUntilMs) {
                showingBootscreen = false;
            } else {
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
        }

        int bug = (ihdg + 45) % 360;
        int course = (ihdg + 30) % 360;
        int bearing = (ihdg + 90) % 360;
        int spd = 140;
        int alt = 6000;
        int vsi = 0;

        /* ---- PFD, pushed to CS1 ---- */
        clearFB(COL_BLACK);
        drawHorizon(pitch, roll);
        drawPitchLadder(pitch, roll);
        drawRollScale();
        drawCoursePointer(ihdg, course);
        drawBearingPointer(ihdg, bearing);
        drawCDIScale();
        drawCDI(course, bearing);
        drawToFromText(course, bearing);
        drawHeadingTape(ihdg, bug);
        drawHeadingWindow(ihdg);
        renderAirspeedTape(spd);
        drawSpeedWindow(spd);
        renderAltitudeTape(alt);
        drawAltitudeWindow(alt);
        drawVSI(vsi);
        drawVSINumber();
        drawAircraft();
        cockpitDisplaySwapAndPush(COCKPIT_PIN_CS1);
        pilotenUpdate(); /* 2. update, while/after CS1 is being pushed */

        /* ---- Moving map, pushed to CS2 ---- */
        clearFB(COL_BLACK);
        trackLat[trackHead] = ownLat;
        trackLon[trackHead] = ownLon;
        trackHead = (trackHead + 1) % TRACK_LEN;
        if (trackHead == 0) trackFilled = true;

        drawMapBackground();
        drawMapTrack(ownLat, ownLon, ihdg);
        drawMapHeadingArrow(ihdg);
        drawMapAircraftChevron(ihdg);
        cockpitDisplaySwapAndPush(COCKPIT_PIN_CS2);
        pilotenUpdate(); /* 3. update, while/after CS2 is being pushed */

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
