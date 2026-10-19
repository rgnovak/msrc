/**************************************************************
 * imu.c - PIMORONI ICM20948 INTERFACE (pico-sdk / RP2350 VERSION)
 * ============================================================
 *
 * Ported from RC-Cockpit's imu.cpp (Arduino, SparkFun ICM_20948
 * Arduino library) to raw pico-sdk hardware_i2c, talking to the
 * vendored SparkFun ICM_20948_C core directly (see
 * icm20948/README.md) instead of through the Arduino wrapper
 * classes (ICM_20948.cpp/.h), which this build doesn't have.
 *
 * Everything that isn't register-level I/O - the complementary
 * filter for pitch/roll, the tilt-compensated magnetometer
 * heading, the heading smoothing filter, the slip indicator, the
 * 180 degree mounting correction, the magnetometer offset
 * handling - is unchanged from the original, including the German
 * comments.
 *************************************************************/

#include "imu.h"

#include <math.h>
#include <string.h>

#include "compat.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "icm20948/AK09916_REGISTERS.h"
#include "icm20948/ICM_20948_C.h"
#include "pico/time.h"

/* ====================================================
 * I2C GLUE (replaces the Arduino Wire-based Serif)
 * ==================================================== */
typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
} imu_i2c_ctx_t;

static imu_i2c_ctx_t imuI2cCtx;
static ICM_20948_Serif_t imuSerif;
static ICM_20948_Device_t icm;

/* RP2350/RP2040 alternate between I2C0 and I2C1 every two GPIOs
 * (0,1 -> I2C0; 2,3 -> I2C1; 4,5 -> I2C0; 6,7 -> I2C1; ...). */
static i2c_inst_t *i2c_instance_for_gpio(uint8_t gpio) { return (((gpio / 2) % 2) == 0) ? i2c0 : i2c1; }

static ICM_20948_Status_e icm_i2c_write(uint8_t regaddr, uint8_t *pdata, uint32_t len, void *user) {
    imu_i2c_ctx_t *ctx = (imu_i2c_ctx_t *)user;
    if (ctx == NULL || len > INV_MAX_SERIAL_WRITE) return ICM_20948_Stat_ParamErr;

    uint8_t buf[1 + INV_MAX_SERIAL_WRITE];
    buf[0] = regaddr;
    memcpy(&buf[1], pdata, len);

    int ret = i2c_write_blocking(ctx->i2c, ctx->addr, buf, (size_t)(len + 1), false);
    return (ret == (int)(len + 1)) ? ICM_20948_Stat_Ok : ICM_20948_Stat_Err;
}

static ICM_20948_Status_e icm_i2c_read(uint8_t regaddr, uint8_t *pdata, uint32_t len, void *user) {
    imu_i2c_ctx_t *ctx = (imu_i2c_ctx_t *)user;
    if (ctx == NULL) return ICM_20948_Stat_ParamErr;

    int wret = i2c_write_blocking(ctx->i2c, ctx->addr, &regaddr, 1, true); /* repeated start */
    if (wret != 1) return ICM_20948_Stat_Err;

    int rret = i2c_read_blocking(ctx->i2c, ctx->addr, pdata, (size_t)len, false);
    return (rret == (int)len) ? ICM_20948_Stat_Ok : ICM_20948_Stat_NoData;
}

/* ====================================================
 * SENSOR-WERTE (AKTUELL)
 * ==================================================== */
static float gPitch = 0.0f;
static float gRoll = 0.0f;
static int gHeading = 0;
static float gSlip = 0.0f;

/* Letztes AGMT-Sample (fuer die Skalierungsfunktionen unten,
 * wie agmt in der Arduino-Wrapper-Klasse). */
static ICM_20948_AGMT_t agmt;

/* ====================================================
 * COMPLEMENTARY FILTER (Pitch/Roll)
 * ==================================================== */
static const float ALPHA = 0.98f;
static uint32_t lastUpdateTimeMs = 0;

/* ====================================================
 * MAGNETOMETER KALIBRIERUNG
 * ==================================================== */
static float magOffsetX = 0.0f;
static float magOffsetY = 0.0f;
static float magOffsetZ = 0.0f;

/* ====================================================
 * HEADING FILTER (fuer sanfte 0/360 Grad Uebergaenge)
 * ==================================================== */
static float headingFiltered = 0.0f;
static const float HEADING_ALPHA = 0.85f;

/* ====================================================
 * HILFSFUNKTIONEN
 * ==================================================== */
static int wrapHeading(int h) {
    while (h < 0) h += 360;
    while (h >= 360) h -= 360;
    return h;
}

/* ====================================================
 * TILT-KOMPENSIERTES HEADING
 * ==================================================== */
static float getTiltCompensatedHeading(float magX, float magY, float magZ, float pitch, float roll) {
    float pitchRad = pitch * DEG_TO_RAD;
    float rollRad = roll * DEG_TO_RAD;

    float cosPitch = cosf(pitchRad);
    float sinPitch = sinf(pitchRad);
    float cosRoll = cosf(rollRad);
    float sinRoll = sinf(rollRad);

    float Xh = magX * cosPitch + magZ * sinPitch;
    float Yh = magX * sinRoll * sinPitch + magY * cosRoll - magZ * sinRoll * cosPitch;

    float heading = atan2f(Yh, Xh) * RAD_TO_DEG;

    if (heading < 0) heading += 360.0f;

    return heading;
}

/* ====================================================
 * SKALIERUNG (ersetzt ICM_20948::accX/gyrX/magX, die in der
 * Arduino-Wrapper-Klasse sitzen - nicht im C-Core).
 * ==================================================== */
static float getAccMG(int16_t axis_val) {
    switch (agmt.fss.a) {
        case 0:
            return ((float)axis_val) / 16.384f;
        case 1:
            return ((float)axis_val) / 8.192f;
        case 2:
            return ((float)axis_val) / 4.096f;
        case 3:
            return ((float)axis_val) / 2.048f;
        default:
            return 0;
    }
}

static float getGyrDPS(int16_t axis_val) {
    switch (agmt.fss.g) {
        case 0:
            return ((float)axis_val) / 131.0f;
        case 1:
            return ((float)axis_val) / 65.5f;
        case 2:
            return ((float)axis_val) / 32.8f;
        case 3:
            return ((float)axis_val) / 16.4f;
        default:
            return 0;
    }
}

static float getMagUT(int16_t axis_val) { return ((float)axis_val) * 0.15f; }

/* ====================================================
 * MAGNETOMETER STARTUP (ersetzt ICM_20948::startupMagnetometer)
 * ==================================================== */
static bool imuStartupMagnetometer(void) {
    ICM_20948_i2c_master_passthrough(&icm, false); /* SDA/SCL nicht an AUX_DA/AUX_CL durchreichen */
    ICM_20948_i2c_master_enable(&icm, true);

    uint8_t srst = 1;
    ICM_20948_i2c_master_single_w(&icm, MAG_AK09916_I2C_ADDR, AK09916_REG_CNTL3, &srst);

    /* Nach einem ICM-Reset reagiert der Mag-Sensor ueber den I2C-Master
     * manchmal erst nach ein paar Versuchen - Master-I2C zuruecksetzen
     * und erneut probieren, bis die WhoAmI-Register stimmen. */
    const int MAX_MAGNETOMETER_STARTS = 10;
    bool found = false;
    for (int tries = 0; tries < MAX_MAGNETOMETER_STARTS; tries++) {
        uint8_t wia1 = 0, wia2 = 0;
        ICM_20948_i2c_master_single_r(&icm, MAG_AK09916_I2C_ADDR, AK09916_REG_WIA1, &wia1);
        ICM_20948_i2c_master_single_r(&icm, MAG_AK09916_I2C_ADDR, AK09916_REG_WIA2, &wia2);

        if (wia1 == (uint8_t)(MAG_AK09916_WHO_AM_I >> 8) && wia2 == (uint8_t)(MAG_AK09916_WHO_AM_I & 0xFF)) {
            found = true;
            break;
        }

        ICM_20948_i2c_master_reset(&icm);
        sleep_ms(10);
    }

    if (!found) {
        debug("\nIMU: Magnetometer (AK09916) WhoAmI nicht erkannt");
        return false;
    }

    AK09916_CNTL2_Reg_t reg;
    reg.MODE = AK09916_mode_cont_100hz;
    reg.reserved_0 = 0;
    ICM_20948_i2c_master_single_w(&icm, MAG_AK09916_I2C_ADDR, AK09916_REG_CNTL2, (uint8_t *)&reg);

    /* Peripheral 0 liest fortlaufend ST1+HXL..HZH+ST2 (9 Byte) aus dem
     * Magnetometer und legt sie in die EXT_SLV_SENS_DATA-Register, wo
     * ICM_20948_get_agmt() sie mit abholt - exakt wie im Original. */
    ICM_20948_i2c_controller_configure_peripheral(&icm, 0, MAG_AK09916_I2C_ADDR, AK09916_REG_ST1, 9, true, true, false,
                                                   false, false, 0);

    return true;
}

/* ====================================================
 * INITIALISIERUNG
 * ==================================================== */
bool imuBegin(uint8_t pinSDA, uint8_t pinSCL) {
    debug("\nIMU: Initialisierung (pico-sdk)");

    imuI2cCtx.i2c = i2c_instance_for_gpio(pinSDA);
    imuI2cCtx.addr = ICM_20948_I2C_ADDR_AD0;

    i2c_init(imuI2cCtx.i2c, 400 * 1000);
    gpio_set_function(pinSDA, GPIO_FUNC_I2C);
    gpio_set_function(pinSCL, GPIO_FUNC_I2C);
    gpio_pull_up(pinSDA);
    gpio_pull_up(pinSCL);
    sleep_ms(100);

    ICM_20948_init_struct(&icm);
    imuSerif.write = icm_i2c_write;
    imuSerif.read = icm_i2c_read;
    imuSerif.user = &imuI2cCtx;
    ICM_20948_link_serif(&icm, &imuSerif);

    /* Adresse 0x68 (Standard) versuchen, sonst 0x69 */
    if (ICM_20948_check_id(&icm) != ICM_20948_Stat_Ok) {
        imuI2cCtx.addr = ICM_20948_I2C_ADDR_AD1;
        if (ICM_20948_check_id(&icm) != ICM_20948_Stat_Ok) {
            debug("\nIMU: FEHLER - ICM-20948 nicht gefunden!");
            return false;
        }
    }
    debug("\nIMU: ICM-20948 verbunden");

    if (ICM_20948_sw_reset(&icm) != ICM_20948_Stat_Ok) {
        debug("\nIMU: FEHLER - swReset fehlgeschlagen");
        return false;
    }
    sleep_ms(50);

    ICM_20948_sleep(&icm, false);
    ICM_20948_low_power(&icm, false);

    if (!imuStartupMagnetometer()) {
        debug("\nIMU: WARNUNG - Magnetometer-Init fehlgeschlagen");
    } else {
        debug("\nIMU: Magnetometer aktiviert");
    }

    ICM_20948_set_sample_mode(&icm, (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), ICM_20948_Sample_Mode_Continuous);

    ICM_20948_fss_t fss;
    fss.a = gpm2;
    fss.g = dps250;
    fss.reserved_0 = 0;
    ICM_20948_set_full_scale(&icm, (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), fss);

    ICM_20948_dlpcfg_t dlpcfg;
    dlpcfg.a = acc_d473bw_n499bw;
    dlpcfg.g = gyr_d361bw4_n376bw5;
    ICM_20948_set_dlpf_cfg(&icm, (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), dlpcfg);

    ICM_20948_enable_dlpf(&icm, ICM_20948_Internal_Acc, false);
    ICM_20948_enable_dlpf(&icm, ICM_20948_Internal_Gyr, false);

    lastUpdateTimeMs = to_ms_since_boot(get_absolute_time());

    debug("\nIMU: bereit");
    return true;
}

/* ====================================================
 * UPDATE (SENSOR-FUSION)
 * ==================================================== */
void imuUpdate(void) {
    if (ICM_20948_data_ready(&icm) != ICM_20948_Stat_Ok) {
        return;
    }

    ICM_20948_get_agmt(&icm, &agmt);

    /* -------- Zeit-Delta -------- */
    uint32_t now = to_ms_since_boot(get_absolute_time());
    float dt = (now - lastUpdateTimeMs) / 1000.0f;
    lastUpdateTimeMs = now;

    /* -------- Sensor-Daten (180 Grad Montage-Korrektur) -------- */
    float ax = -getAccMG(agmt.acc.axes.x);
    float ay = getAccMG(agmt.acc.axes.y); /* NICHT negieren - sonst ist Pitch invertiert! */
    float az = -getAccMG(agmt.acc.axes.z);

    float gx = -getGyrDPS(agmt.gyr.axes.x);
    float gy = getGyrDPS(agmt.gyr.axes.y); /* NICHT negieren - sonst ist Pitch invertiert! */
    float gz = -getGyrDPS(agmt.gyr.axes.z);

    float mx = -getMagUT(agmt.mag.axes.x) - magOffsetX;
    float my = -getMagUT(agmt.mag.axes.y) - magOffsetY;
    float mz = -getMagUT(agmt.mag.axes.z) - magOffsetZ;

    /* -------- Pitch/Roll aus Beschleunigung -------- */
    float pitchAcc = atan2f(ay, sqrtf(ax * ax + az * az)) * RAD_TO_DEG;
    float rollAcc = atan2f(-ax, az) * RAD_TO_DEG;

    /* -------- Pitch/Roll aus Gyro (Integration) -------- */
    float pitchGyro = gPitch + gy * dt;
    float rollGyro = gRoll + gx * dt;

    /* -------- Complementary Filter -------- */
    gPitch = ALPHA * pitchGyro + (1.0f - ALPHA) * pitchAcc;
    gRoll = ALPHA * rollGyro + (1.0f - ALPHA) * rollAcc;

    /* -------- Tilt-kompensiertes Heading -------- */
    float heading = getTiltCompensatedHeading(mx, my, mz, gPitch, gRoll);

    /* -------- Heading Filter (sanfte 0/360 Grad Uebergaenge) -------- */
    float delta = heading - headingFiltered;
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;

    headingFiltered += delta * (1.0f - HEADING_ALPHA);
    if (headingFiltered < 0) headingFiltered += 360.0f;
    if (headingFiltered >= 360.0f) headingFiltered -= 360.0f;

    gHeading = (int)(headingFiltered + 0.5f);
    gHeading = wrapHeading(gHeading);

    /* -------- Slip Indikator -------- */
    gSlip = constrain(gz / 100.0f, -1.0f, 1.0f);
}

/* ====================================================
 * GETTER
 * ==================================================== */
float imuGetPitch(void) { return gPitch; }
float imuGetRoll(void) { return gRoll; }
int imuGetHeading(void) { return gHeading; }
float imuGetSlip(void) { return gSlip; }
