/**************************************************************
 * PILOTEN MODULE - IMPLEMENTATION (RP2350 / pico-sdk VERSION)
 *
 * Ported from RC-Cockpit's piloten.cpp (Arduino, Servo library,
 * two servos) to raw RP2350 hardware_pwm, single servo (the
 * Core2350B pilot head has one gaze servo, not two - see
 * piloten.h).
 *
 * Behavior is kept faithful to the original "PILOT 1" half of the
 * two-servo FSM: same constants, same random-timing/easing logic,
 * same roll-dependent look-into-the-turn bias. Only the servo
 * output stage changed - degrees are still generated exactly as
 * before and converted to a pulse width in microseconds right
 * before writing to the PWM peripheral, replicating the Arduino
 * Servo library's own default 0-180 degree -> 544-2400 microsecond
 * mapping so the physical motion matches the original build.
 **************************************************************/

#include "piloten.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "compat.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"

/* ==========================================================
 * SERVO GRUNDKONFIGURATION
 * ========================================================== */
static const int SERVO_MIN = 10;   // mechanisches Minimum (Grad)
static const int SERVO_MAX = 160;  // mechanisches Maximum (Grad)

/* Bewegungsparameter */
static const float SPEED_MIN = 8.0f;   // ms pro Grad (schnell)
static const float SPEED_MAX = 32.0f;  // ms pro Grad (langsam)

static const int PAUSE_MIN = 600;   // ms
static const int PAUSE_MAX = 2400;  // ms

/* Arduino Servo library default pulse-width range (0-180 degrees). */
static const int SERVO_PULSE_MIN_US = 544;
static const int SERVO_PULSE_MAX_US = 2400;

/* ==========================================================
 * HARDWARE PWM (50 Hz, 1 tick = 1 microsecond)
 * ========================================================== */
static uint8_t servoGpio = 0;
static uint servoSlice = 0;

static void pwmServoBegin(uint8_t gpio) {
    servoGpio = gpio;

    gpio_set_function(gpio, GPIO_FUNC_PWM);
    servoSlice = pwm_gpio_to_slice_num(gpio);

    /* 1 tick = 1 us, wrap at 20000 ticks -> 20 ms period (50 Hz),
     * same as the Arduino Servo library's own refresh rate. Divider
     * is derived from the actual system clock rather than hardcoded,
     * matching the pattern already used elsewhere in this codebase
     * (see sensor/pwm_out.c). */
    float clkdiv = (float)clock_get_hz(clk_sys) / 1000000.0f;

    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv(&c, clkdiv);
    pwm_config_set_wrap(&c, 20000 - 1);
    pwm_init(servoSlice, &c, true);
}

/* ==========================================================
 * SERVO CONTROLLER (single instance - one gaze servo)
 * ========================================================== */
typedef struct {
    float currentAngle;
    float startAngle;
    float targetAngle;
    float msPerDegree;
    uint32_t startTime;
    uint32_t duration;
    bool moving;
} ServoController;

static ServoController SC;

/* ==========================================================
 * ROLL-INPUT (vom IMU-Modul)
 * ========================================================== */
static float pilotRollDeg = 0.0f;

/* Wird aus dem Cockpit-Task gesetzt */
void pilotenSetRoll(float rollDeg) { pilotRollDeg = constrain(rollDeg, -45.0f, 45.0f); }

/* ==========================================================
 * RUDDER-INPUT (vom SRXL2-Kanal, ueber den Cockpit-Task)
 * ========================================================== */
static float pilotRudderNorm = 0.0f;

void pilotenSetRudder(float rudderNorm) { pilotRudderNorm = constrain(rudderNorm, -1.0f, 1.0f); }

/* ==========================================================
 * ENABLE / DISABLE
 * ========================================================== */
static bool headEnabled = true;
static bool parking = false; /* unterwegs zur reinen Neutralstellung (siehe pilotenUpdate) */

void pilotenSetEnabled(bool enabled) { headEnabled = enabled; }
bool pilotenGetEnabled(void) { return headEnabled; }

/* ==========================================================
 * ZUFALLSZAHLEN (ersetzt Arduinos random())
 * ========================================================== */
static bool rndSeeded = false;

/* Returns an integer in [lo, hi), matching Arduino's random(min,max). */
static long rnd_range(long lo, long hi) {
    if (!rndSeeded) {
        srand((unsigned)to_ms_since_boot(get_absolute_time()));
        rndSeeded = true;
    }
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo));
}

/* ==========================================================
 * HILFSFUNKTIONEN
 * ========================================================== */
static float ease(float t) {
    // Smoothstep / S-Kurve
    return t * t * (3.0f - 2.0f * t);
}

/* Roll -> Blick-Bias */
static int rollBias(void) {
    const float MAX_BIAS = 12.0f;  // max. Servo-Grad durch Roll
    return (int)(pilotRollDeg / 45.0f * MAX_BIAS);
}

/* Ruder/Gier -> Blick-Bias (zusaetzlich zur Roll-Bias). Vorzeichen
 * ist eine Annahme (rechtes Ruder -> Blick nach rechts, wie bei der
 * Roll-Bias) - ohne Board/Sender zum Testen nicht verifizierbar.
 * Falls es am echten Modell seitenverkehrt ist: Vorzeichen hier
 * einfach umdrehen. */
static int rudderBias(void) {
    const float MAX_BIAS = 10.0f;  // max. Servo-Grad durch Ruder
    return (int)(pilotRudderNorm * MAX_BIAS);
}

/* ==========================================================
 * SERVO WRITE (RP2350 hardware_pwm)
 * ========================================================== */
static void servoWrite(int angle) {
    angle = constrain(angle, SERVO_MIN, SERVO_MAX);

    /* Same 0-180 degree -> 544-2400 us mapping the Arduino Servo
     * library uses by default, so the physical travel matches the
     * previously-working reference build. */
    int pulse_us = map(angle, 0, 180, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
    pwm_set_gpio_level(servoGpio, (uint16_t)pulse_us);
}

/* ==========================================================
 * BEWEGUNG STARTEN
 * ========================================================== */
static void moveTo(ServoController *sc, int angle) {
    angle = constrain(angle, SERVO_MIN, SERVO_MAX);

    if (!isfinite(sc->currentAngle)) {
        sc->currentAngle = (float)angle;
    }

    sc->startAngle = sc->currentAngle;
    sc->targetAngle = (float)angle;

    float delta = fabsf(sc->targetAngle - sc->startAngle);

    sc->msPerDegree = rnd_range((long)(SPEED_MIN * 100), (long)(SPEED_MAX * 100)) / 100.0f;

    uint32_t computed = (uint32_t)(delta * sc->msPerDegree);
    sc->duration = computed > 80 ? computed : 80;

    sc->startTime = to_ms_since_boot(get_absolute_time());
    sc->moving = true;
}

/* ==========================================================
 * BEWEGUNG UPDATE (NON-BLOCKING)
 * ========================================================== */
static void updateMovement(ServoController *sc) {
    if (!sc->moving) return;

    uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - sc->startTime;

    if (elapsed >= sc->duration) {
        servoWrite((int)sc->targetAngle);
        sc->currentAngle = sc->targetAngle;
        sc->moving = false;
        return;
    }

    float t = (float)elapsed / (float)sc->duration;
    float pos = sc->startAngle + (sc->targetAngle - sc->startAngle) * ease(t);

    servoWrite((int)pos);
    sc->currentAngle = pos;
}

/* ==========================================================
 * NEUTRALPOSITION
 * ========================================================== */
static int neutral = 90;

static void updateNeutral(int *n) {
    int base = 90 + rollBias() + rudderBias();
    *n = constrain(base + rnd_range(-5, 6), SERVO_MIN + 10, SERVO_MAX - 10);
}

/* ==========================================================
 * FSM STATUS
 * ========================================================== */
static bool seqRunning = false;
static int seqStage = 0;
static uint32_t seqWaitUntil = 0;
static int seqA, seqB;
static uint32_t nextAction = 0;

/* ==========================================================
 * BLICK-SEQUENZ
 * ========================================================== */
static void startGazeSequence(ServoController *sc, int *n, int *stage, bool *running, int *tA, int *tB) {
    (void)sc;
    int left = constrain(*n - rnd_range(15, 40), SERVO_MIN, SERVO_MAX);
    int right = constrain(*n + rnd_range(15, 40), SERVO_MIN, SERVO_MAX);

    if (rnd_range(0, 2) == 0) {
        *tA = left;
        *tB = right;
    } else {
        *tA = right;
        *tB = left;
    }

    *stage = 1;
    *running = true;
}

/* ==========================================================
 * PUBLIC API
 * ========================================================== */
void pilotenBegin(uint8_t gpio) {
    pwmServoBegin(gpio);

    SC.currentAngle = NAN;
    SC.startAngle = 0;
    SC.targetAngle = 0;
    SC.msPerDegree = 0;
    SC.startTime = 0;
    SC.duration = 0;
    SC.moving = false;

    updateNeutral(&neutral);
    moveTo(&SC, neutral);

    nextAction = to_ms_since_boot(get_absolute_time()) + 300;
}

void pilotenUpdate(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    updateMovement(&SC);

    if (!headEnabled) {
        /* Abgeschaltet: einmalig auf reine Neutralstellung (90 Grad,
         * ohne Roll-/Ruder-Bias) fahren und dort halten. Laufende
         * Blicksequenz/FSM wird eingefroren, nicht fortgesetzt. */
        if (!parking) {
            seqRunning = false;
            moveTo(&SC, 90);
            parking = true;
        }
        return;
    }

    if (parking) {
        /* Gerade wieder aktiviert: FSM ab jetzt frisch starten. */
        parking = false;
        nextAction = now + rnd_range(PAUSE_MIN, PAUSE_MAX);
    }

    if (!seqRunning && now >= nextAction) {
        nextAction = now + rnd_range(PAUSE_MIN, PAUSE_MAX);

        if (rnd_range(0, 100) < 25) {
            startGazeSequence(&SC, &neutral, &seqStage, &seqRunning, &seqA, &seqB);
        } else {
            moveTo(&SC, (int)rnd_range(SERVO_MIN, SERVO_MAX));
            seqStage = 10;
            seqRunning = true;
        }
    }

    if (seqRunning) {
        if (seqStage == 1 && !SC.moving) {
            moveTo(&SC, seqA);
            seqStage = 2;
            seqWaitUntil = now + rnd_range(150, 350);
        } else if (seqStage == 2 && !SC.moving && now >= seqWaitUntil) {
            moveTo(&SC, seqB);
            seqStage = 3;
            seqWaitUntil = now + rnd_range(150, 350);
        } else if ((seqStage == 3 || seqStage == 10) && !SC.moving) {
            updateNeutral(&neutral);
            moveTo(&SC, neutral);
            seqRunning = false;
        }
    }
}
