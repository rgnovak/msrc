#include "cockpit_task.h"

#include <math.h>

#include "bootscreen.h"
#include "compat.h"
#include "display.h"
#include "map.h"
#include "pfd.h"
#include "piloten.h"

/* Owns both panels end to end: brings up the SPI driver and
 * framebuffers, shows the boot splash once, then renders the PFD
 * (CS1) and the moving map (CS2) in a loop, while also driving the
 * pilot head's gaze servo.
 *
 * Nothing here reads real sensor or SRXL2 data yet - pitch/roll/
 * heading are a gentle synthetic sweep, purely so the panels
 * visibly come alive once this is flashed, standing in for the IMU
 * work that comes next (the servo gaze module itself is already the
 * real port, just fed a synthetic roll for now). Airspeed/altitude/
 * VSI reuse the same fixed demo values the original Arduino sketch
 * used, and ownLat/ownLon (in map.c) never change yet either, so the
 * map's breadcrumb track stays put - also matching the original
 * demo. */

void cockpit_task(void *parameters) {
    (void)parameters;

    cockpitDisplayBegin();
    pilotenBegin(COCKPIT_PIN_PILOT_SERVO);

    showGarminBootscreen();
    cockpitDisplayPushBootscreen();
    vTaskDelay(pdMS_TO_TICKS(4000));

    float heading = 0.0f;

    while (1) {
        heading += 1.0f;
        if (heading >= 360.0f) heading -= 360.0f;

        float pitch = 15.0f * sinf(heading * DEG_TO_RAD);
        float roll = 25.0f * sinf(2.0f * heading * DEG_TO_RAD);

        pilotenSetRoll(roll);
        pilotenUpdate(); /* 1. update, before rendering - matches the
                           * original Arduino loop's "update once, then
                           * once more after each panel push" pattern,
                           * trimmed from 4 panels to this board's 2. */

        int ihdg = (int)heading;
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
