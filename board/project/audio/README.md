# Cockpit-Audio (not yet ported)

Placeholder for the Cockpit-Audio project (ESP32 + exciter module)
once it's ported onto the same RP2350 chip, instead of running on its
own separate ESP32.

Gated behind the `MSRC_ENABLE_AUDIO` CMake option (default OFF), set
in `board/project/CMakeLists.txt` next to `MSRC_ENABLE_COCKPIT` and
`MSRC_ENABLE_LIGHT_CONTROLLER` - independent of the cockpit display/
servo/IMU, so a model like the J65 (no cockpit display) can still
enable just this one. The same value is also available in C as the
`MSRC_ENABLE_AUDIO` preprocessor macro (0 or 1), for any shared file
(e.g. `main.c`) that needs to `#if` around code this subsystem needs.

Reserved pins (see `cockpit/cockpit_pins.h`'s pin-budget comment):
GPIO 44-46, clear of MSRC's own pins and of the cockpit display/
servo/IMU pins - exact assignment (I2S BCLK/LRC/DATA or similar) once
this is actually designed.

Until real source files land here, this directory only holds a
`CMakeLists.txt` that fails the build with a clear message if
`MSRC_ENABLE_AUDIO` is accidentally switched ON - turning it on is a
reminder to come write this part, not something that silently
compiles to nothing.
