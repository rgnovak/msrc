# Light controller (not yet built)

Placeholder for a light-controller subsystem: beacon, landing light,
strobe/flasher, and position lights, each driven as a simple PWM or
on/off signal into an external LED driver (no LED current driven
directly off a GPIO). Pattern generation would reuse the same
non-blocking FSM style already used for the pilot-head gaze servo
(`cockpit/piloten.c`), just producing light patterns instead of servo
angles.

Gated behind the `MSRC_ENABLE_LIGHT_CONTROLLER` CMake option (default
OFF), set in `board/project/CMakeLists.txt` next to
`MSRC_ENABLE_COCKPIT` and `MSRC_ENABLE_AUDIO` - independent of the
cockpit display/servo/IMU, so a model like the J65 (no cockpit
display) can still enable just this one. The same value is also
available in C as the `MSRC_ENABLE_LIGHT_CONTROLLER` preprocessor
macro (0 or 1).

No pins reserved yet - channel count isn't decided (4 for beacon/
landing light/strobe/position, maybe 5 if position is split left/
right), so there's nothing to pin down. There's comfortably enough
free GPIO left once this is designed (see the pin-budget comment at
the top of `cockpit/cockpit_pins.h` for what's already taken).

Until real source files land here, this directory only holds a
`CMakeLists.txt` that fails the build with a clear message if
`MSRC_ENABLE_LIGHT_CONTROLLER` is accidentally switched ON.
