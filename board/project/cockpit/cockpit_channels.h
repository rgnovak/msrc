#ifndef COCKPIT_CHANNELS_H
#define COCKPIT_CHANNELS_H

/* SRXL2 channel assignment for the cockpit's RC inputs, numbered the
 * same way the transmitter/receiver shows them (1-indexed) - just
 * change these two lines if the switch or the rudder end up on a
 * different channel on your transmitter, nothing else needs to
 * know about it.
 *
 * COCKPIT_CH_SWITCH: the 3-position switch that used to be read
 * over a dedicated PWM wire (pwm_input.cpp) - now read as an SRXL2
 * channel instead, same toggle behavior (see cockpit_switch.h).
 * Channel 5 per Ralph: free on the Rockstar since it has no
 * retractable gear.
 *
 * COCKPIT_CH_RUDDER: fed into the pilot head's gaze servo as an
 * extra look-bias (see piloten.h's pilotenSetRudder()). Channel 4
 * matches Ralph's own channel convention on the ix14+SE/AR10360T+
 * (K4 = Seitenruder, per the A320 setup) - not yet confirmed for
 * the Rockstar specifically, so double-check once the transmitter
 * is in front of you. */
#define COCKPIT_CH_SWITCH 5
#define COCKPIT_CH_RUDDER 4

#endif /* COCKPIT_CHANNELS_H */
