#ifndef COCKPIT_PILOTEN_H
#define COCKPIT_PILOTEN_H

/* Ported from RC-Cockpit's piloten.cpp/.h (Arduino sketch): drives
 * the pilot head's gaze servo with realistic, non-blocking
 * random look-around movements plus a roll-dependent bias (the
 * pilot looks into the turn) and a rudder-dependent bias (the
 * pilot also looks toward the rudder/yaw input).
 *
 * The reference build this was first validated on (a 4-display
 * Waveshare board) had two pilot servos; the Core2350B's 2-display
 * layout has one (see pwm_input.* in the original sketch), so this
 * keeps a single servo controller rather than porting the second,
 * unused one. */

#include <stdbool.h>
#include <stdint.h>

#define COCKPIT_PIN_PILOT_SERVO 21

void pilotenBegin(uint8_t gpio);
void pilotenUpdate(void);

/* Roll angle in degrees, e.g. -45..+45. Feeds the look-into-the-turn
 * bias; call before pilotenUpdate() each frame. */
void pilotenSetRoll(float rollDeg);

/* Rudder/yaw input, normalized -1.0 (full left) .. +1.0 (full
 * right). Feeds a second, independent look-bias on top of the roll
 * bias; call before pilotenUpdate() each frame. */
void pilotenSetRudder(float rudderNorm);

/* Head movement on/off. When disabled the servo moves once to pure
 * neutral (90 degrees, no roll/rudder bias) and holds there instead
 * of continuing the random gaze FSM; re-enabling resumes the FSM
 * from wherever the servo currently is. */
void pilotenSetEnabled(bool enabled);
bool pilotenGetEnabled(void);

#endif /* COCKPIT_PILOTEN_H */
