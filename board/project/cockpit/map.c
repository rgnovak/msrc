#include "map.h"

#include <math.h>

#include "compat.h"
#include "map_bitmap.h"
#include "pfd.h"

/* Ported near-verbatim from RC-Cockpit's map.cpp - moving map with a
 * breadcrumb track and an aircraft chevron, track-up relative to
 * heading. The only real change from the Arduino original is reading
 * mapBitmap directly instead of through pgm_read_word(): RP2350 has a
 * single unified address space, so there is no separate flash-read
 * path to go through the way there is on AVR. */

/* Own position (demo values for now; will come from GPS/telemetry
 * later). */
float ownLat = 48.1234f;
float ownLon = 16.9876f;

/* Track history (breadcrumbs). */
float trackLat[TRACK_LEN];
float trackLon[TRACK_LEN];
uint8_t trackHead = 0;
bool trackFilled = false;

void drawMapBackground(void) {
    for (int i = 0; i < W * H; i++) backFB[i] = mapBitmap[i];
}

void drawMapTrack(float lat, float lon, int heading) {
    const float metersPerPixel = 6.0f;  // zoom factor
    float hdg = heading * DEG_TO_RAD;

    int count = trackFilled ? TRACK_LEN : trackHead;

    for (int i = 0; i < count; i++) {
        /* Difference in geographic coordinates. */
        float dLat = trackLat[i] - lat;
        float dLon = trackLon[i] - lon;

        /* Converted to meters. */
        float dy = dLat * 110540.0f;
        float dx = dLon * 111320.0f * cosf(lat * DEG_TO_RAD);

        /* Rotated into heading direction (track-up). */
        float rx = dx * cosf(hdg) + dy * sinf(hdg);
        float ry = -dx * sinf(hdg) + dy * cosf(hdg);

        /* Scaled to display coordinates. */
        int x = CX + rx / metersPerPixel;
        int y = CY - ry / metersPerPixel;

        if (x > 1 && x < W - 2 && y > 1 && y < H - 2) px(x, y, COL_GREEN);
    }
}

void drawMapHeadingArrow(int heading) {
    float a = heading * DEG_TO_RAD;

    for (int i = 0; i < 30; i++) {
        int x = CX + sinf(a) * i;
        int y = CY - cosf(a) * i;

        if (x > 0 && x < W && y > 0 && y < H) px(x, y, COL_MAGENTA);
    }
}

/* Aircraft symbol: double chevron, heading-rotated (track-up). */
void drawMapAircraftChevron(int heading) {
    const int sizeOuter = 25;
    const int sizeInner = 20;
    const int wingAngle = 150;  // opening angle, degrees

    float a = heading * DEG_TO_RAD;

    float aL = a + wingAngle * DEG_TO_RAD;
    float aR = a - wingAngle * DEG_TO_RAD;

    int cx = CX;
    int cy = CY;

    for (int i = 0; i <= sizeOuter; i++) {
        int xl = cx + sinf(aL) * i;
        int yl = cy - cosf(aL) * i;
        int xr = cx + sinf(aR) * i;
        int yr = cy - cosf(aR) * i;

        if (xl > 0 && xl < W && yl > 0 && yl < H) px(xl, yl, COL_YELLOW);
        if (xr > 0 && xr < W && yr > 0 && yr < H) px(xr, yr, COL_YELLOW);
    }

    for (int i = 0; i <= sizeInner; i++) {
        int xl = cx + sinf(aL) * i;
        int yl = cy - cosf(aL) * i;
        int xr = cx + sinf(aR) * i;
        int yr = cy - cosf(aR) * i;

        if (xl > 0 && xl < W && yl > 0 && yl < H) px(xl, yl, COL_YELLOW);
        if (xr > 0 && xr < W && yr > 0 && yr < H) px(xr, yr, COL_YELLOW);
    }

    /* Nose, drawn double-wide for visibility. */
    for (int i = 0; i < 6; i++) {
        int xn = cx + sinf(a) * i;
        int yn = cy - cosf(a) * i;

        if (xn > 0 && xn < W && yn > 0 && yn < H) px(xn, yn, COL_YELLOW);
        if (xn + 1 > 0 && xn + 1 < W && yn > 0 && yn < H) px(xn + 1, yn, COL_YELLOW);
    }
}
