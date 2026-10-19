#ifndef COCKPIT_IMU_H
#define COCKPIT_IMU_H

/**************************************************************
 * imu.h - PIMORONI ICM20948 INTERFACE (pico-sdk / RP2350 port)
 * ============================================================
 *
 * Ported from RC-Cockpit's imu.cpp/.h (Arduino, SparkFun ICM_20948
 * Arduino library) to raw pico-sdk hardware_i2c + the vendored
 * SparkFun ICM_20948_C core in icm20948/ (see icm20948/README.md).
 * The sensor-fusion math itself (complementary filter for pitch/
 * roll, tilt-compensated magnetometer heading, slip indicator) is
 * unchanged from the original.
 *
 * HARDWARE:
 * - Pimoroni ICM20948 9DoF (PIM448)
 * - Montage: 180 Grad gedreht, Z-Achse nach oben
 *
 * OUTPUT:
 * - Pitch:   -90 .. +90 Grad (Nase hoch = positiv)
 * - Roll:    -180 .. +180 Grad (rechts = positiv)
 * - Heading: 0 .. 360 Grad (Nord = 0, im Uhrzeigersinn)
 * - Slip:    -1.0 .. +1.0 (normiert)
 *
 * VERWENDUNG:
 *   imuBegin(PIN_IMU_SDA, PIN_IMU_SCL);  // Einmalig beim Start
 *   imuUpdate();                        // Zyklisch pro Frame
 *
 *   float pitch = imuGetPitch();
 *   float roll = imuGetRoll();
 *   int heading = imuGetHeading();
 *   float slip = imuGetSlip();
 *************************************************************/

#include <stdbool.h>
#include <stdint.h>

#include "cockpit_pins.h" /* COCKPIT_PIN_IMU_SDA / _SCL */
#include "common.h"

extern context_t context;

/* Initialisiert I2C-Bus und ICM-20948 (inkl. Magnetometer).
 * Gibt false zurueck, wenn der Sensor nicht gefunden wurde. */
bool imuBegin(uint8_t pinSDA, uint8_t pinSCL);

/* Liest Sensoren und aktualisiert den Komplementaerfilter. Muss
 * regelmaessig aufgerufen werden (50-200 Hz). */
void imuUpdate(void);

float imuGetPitch(void);
float imuGetRoll(void);
int imuGetHeading(void);
float imuGetSlip(void);

#endif /* COCKPIT_IMU_H */
