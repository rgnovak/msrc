#include "cockpit_task.h"

#include "bootscreen.h"
#include "common.h"
#include "display.h"
#include "imu.h"
#include "map.h"
#include "pfd.h"
#include "piloten.h"

/* Owns both panels end to end: brings up the SPI driver and
 * framebuffers, shows the boot splash once, then renders the PFD
 * (CS1) and the moving map (CS2) in a loop, while also driving the
 * pilot head's gaze servo from real IMU attitude data.
 *
 * Pitch/roll/heading now come from the ICM-20948 IMU (see imu.c) -
 * no more synthetic sweep. If the sensor isn't found at startup
 * (not wired up yet, or a bad connection), imuBegin() logs it and
 * the loop keeps running with pitch/roll/heading simply at 0 rather
 * than crashing, so the rest of the panel - map, airspeed/altitude/
 * VSI, pilot head - still comes up. Airspeed/altitude/VSI still
 * reuse the same fixed demo values the original Arduino sketch
 * used (no ESC/airspeed sensor port yet), and ownLat/ownLon (in
 * map.c) never change yet either, so the map's breadcrumb track
 * stays put - also matching the original demo. */

void cockpit_task(void *parameters) {
    (void)parameters;

    cockpitDisplayBegin();
    pilotenBegin(COCKPIT_PIN_PILOT_SERVO);

    if (!imuBegin(COCKPIT_PIN_IMU_SDA, COCKPIT_PIN_IMU_SCL)) {
        debug("\ncockpit_task: IMU nicht gefunden - Pitch/Roll/Heading bleiben auf 0");
    }

    showGarminBootscreen();
    cockpitDisplayPushBootscreen();
    vTaskDelay(pdMS_TO_TICKS(4000));

    while (1) {
        imuUpdate();

        float pitch = imuGetPitch();
        float roll = imuGetRoll();
        int ihdg = imuGetHeading();

        pilotenSetRoll(roll);
        pilotenUpdate(); /* 1. update, before rendering - matches the
                           * original Arduino loop's "update once, then
                           * once more after each panel push" pattern,
                           * trimmed from 4 panels to this board's 2. */

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
