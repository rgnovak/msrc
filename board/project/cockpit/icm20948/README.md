# Vendored: SparkFun ICM-20948 portable C core

These files are the platform-independent `src/util/` core from
SparkFun's `SparkFun_ICM-20948_ArduinoLibrary`
(https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary),
copied in unmodified. They implement the register-level protocol for
the ICM-20948 9-DoF IMU (plus its onboard AK09916 magnetometer) behind
a small C vtable (`ICM_20948_Serif_t`, see `ICM_20948_C.h`) rather than
Arduino's `Wire`/`SPI` classes, so they drop into a plain pico-sdk/C
build without the Arduino wrapper classes (`ICM_20948.cpp/.h`) that
normally sit on top of them.

`../imu.c` links a `Serif_t` backed by pico-sdk's `hardware_i2c` and
reimplements the handful of wrapper-class methods RC-Cockpit's
`imu.cpp` used (`begin`, `startupMagnetometer`, `dataReady`, `getAGMT`,
`accX/Y/Z`, `gyrX/Y/Z`, `magX/Y/Z`) directly against this core.

DMP (the chip's on-board Digital Motion Processor) support is left
disabled (`ICM_20948_USE_DMP` is not defined), exactly as in the
original Arduino sketch - only the raw accelerometer/gyroscope/
magnetometer path is used, so the large DMP firmware blob that
`ICM_20948_C.c` would otherwise pull in is never included.

License: SparkFun's code is MIT-licensed (see upstream repo's
`License.md`); this notice stands in for copying that file verbatim
into every vendored source file.

  The MIT License (MIT)
  Copyright (c) 2016 SparkFun Electronics
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
