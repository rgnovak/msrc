#ifndef COCKPIT_MAP_H
#define COCKPIT_MAP_H

/* Ported from RC-Cockpit's map.h (Arduino sketch). */

#include <stdbool.h>
#include <stdint.h>

#define TRACK_LEN 64

extern float trackLat[TRACK_LEN];
extern float trackLon[TRACK_LEN];
extern uint8_t trackHead;
extern bool trackFilled;

extern float ownLat;
extern float ownLon;

void drawMapBackground(void);
void drawMapTrack(float lat, float lon, int heading);
void drawMapHeadingArrow(int heading);
void drawMapAircraftChevron(int heading);

#endif /* COCKPIT_MAP_H */
