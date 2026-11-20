# ssc32

[![CI](https://github.com/MartinPeris/ssc32/actions/workflows/ci.yml/badge.svg)](https://github.com/MartinPeris/ssc32/actions/workflows/ci.yml)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

A small, dependency-free C++17 library for the Lynxmotion SSC-32 and SSC-32U
serial protocol. It is designed as a reusable foundation for robot middleware,
including `ros2_control` hardware interfaces, without taking a dependency on
ROS.

## Status

The protocol encoder and controller behavior are covered by automated tests.
Physical SSC-32 and SSC-32U hardware validation is pending. Until that validation
is complete, treat the API as pre-1.0 and verify pulse limits with servos unloaded.

## Features

- Synchronized multi-servo moves with optional speed and duration.
- Pulse offsets, discrete outputs, and byte-bank output.
- Movement-status, pulse-width, and firmware-version queries.
- RAII POSIX serial transport with bounded reads and explicit timeouts.
- An injectable transport interface for deterministic tests and integrations.
- CMake package export for `find_package(ssc32 CONFIG REQUIRED)`.

## Build and test

```sh
cmake -S . -B build -DSSC32_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Example

```cpp
#include <chrono>
#include <ssc32/controller.hpp>
#include <ssc32/serial_transport.hpp>

using namespace std::chrono_literals;

ssc32::SerialTransport serial("/dev/ttyUSB0", 115200);
ssc32::Controller controller(serial);

controller.move({
    {.channel = 0, .pulse_width_us = 1500, .speed_us_per_second = 500},
    {.channel = 1, .pulse_width_us = 1750},
}, 1000ms);
```

The designated initializers in the example require C++20. With C++17, construct
`ServoCommand` values positionally or assign their fields explicitly.

## Safety

SSC-32 controllers command open-loop hobby servos. A pulse-width query reports
the controller command, not measured shaft position. Start with conservative
pulse limits, remove mechanical loads during calibration, provide an accessible
power cutoff, and never rely on this library for a safety function.

## Acknowledgment

This library is a focused extraction and modernization of the SSC-32 protocol
implementation from
[`smd-ros-devel/lynxmotion_ssc32u`](https://github.com/smd-ros-devel/lynxmotion_ssc32u),
originally written by Matt Richard. The original project supplied the protocol
coverage and practical device behavior that made this standalone version
possible. Its BSD 3-Clause license and copyright notice are preserved in
[LICENSE](LICENSE).

The extraction was based on upstream commit
[`239d45b`](https://github.com/smd-ros-devel/lynxmotion_ssc32u/commit/239d45b493f91f6ffc28034ae79cb9e67d4f137d),
primarily `lynxmotion_ssc32u_driver/include/lynxmotion_ssc32u_driver/ssc32.hpp`
and `lynxmotion_ssc32u_driver/src/ssc32.cpp`.

The extraction removes ROS-specific code, introduces a transport boundary,
replaces fixed buffers and unchecked C string operations, handles partial I/O,
adds bounded timeouts and validation, and provides automated tests and installable
CMake targets.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports should include the controller
model, firmware version, host operating system, serial adapter, baud rate, and a
minimal reproduction. Do not include serial-device logs containing unrelated
system information.

## License

BSD 3-Clause. See [LICENSE](LICENSE).
