# Verification record

Completed on 22 October 2026.

## Preserved originals

The original RMI intermediate `Task3.ino` was copied unchanged to `archive/Task3.ino`. The circuit beside that sketch was copied to `proteus/elevator-control.pdsprj`. A different project file from the intermediate-task parent folder was preserved as `archive/alternate-elevator.pdsprj`; its revision order was not inferred.

All three files match their original SHA-256 hashes. Both circuit ZIP archives passed CRC checks and their `PROJECT.XML` entries parsed. Hashes, sizes and archive contents are recorded in `source-inventory.json`.

Stored main-circuit strings contain ATmega328P/Arduino, MCP23017, SSD1306 and L293D model references. The saved Program File refers to the original Task3 Uno HEX in an absolute local build path. Component strings and archive validity do not establish complete wiring or simulation behavior.

## Source findings and preparation fixes

The original source has a compile-blocking `unit16_t` typo. Its transit logic stops only for requests matching the current direction or an internal request. For example, a floor-5 down call while travelling upward can fail to stop at floor 5 and allow the floor counter to exceed its request-array bounds. Served requests are also left queued at a transit arrival, causing another door-open cycle when the controller returns to idle. Repeated level polling can relatch a button held after service.

The prepared firmware corrects these cases with typed GPIO reads, press-edge handling, turnaround service, immediate arrival clearing and floor guards. It also updates PWM for changing load input during transit. The original remains available separately; these fixes are not attributed to its historical behavior.

## Native compilation and execution

Compiled `tests/controller_tests.cpp` with MinGW g++ 15.2.0 on Windows using C++17 and `-Wall -Wextra -Werror`. Compilation succeeded without warnings, then the executable passed ten checks:

1. All five internal and eight hall-button mappings.
2. Two-second floor travel, three-second door timing, arrival clearing, held-button suppression and release/repress handling.
3. A top-floor down call while travelling up, and a bottom-floor up call while travelling down.
4. Passing an opposite-direction call while requests remain ahead, then serving it on the return trip.
5. Serving an opposite-direction call at the current turnaround floor before departing.
6. A new internal request received during transit.
7. PWM changing from 150 to 255 during transit as the simulated load ADC changes from 0 to 1023.
8. Explicit upward/downward start guards at floors 5 and 1.
9. Correct two-second transition across 32-bit `millis()` rollover.
10. A 10,000-tick seeded request sequence, checking floor bounds and stopped motor output outside transit. After request injection ended, the remaining queue drained and the controller returned to idle with zero PWM.

The deterministic request sequence uses `std::mt19937` seed 2841 and 250 ms clock advances. The harness includes the actual prepared `.ino`; it does not reimplement the scheduler in a separate test model.

Fakes replace Arduino functions, clock, ADC/input values, I2C devices, display methods and motor writes. The native test does not verify the target's actual libraries, display content, bus timing or electrical behavior. On hosts with 64-bit `unsigned long`, the Arduino-width rollover check explicitly skips and the executable reports nine passing checks; all ten ran on the Windows test host.

## Arduino Uno build attempt

Attempted to compile the prepared sketch for `arduino:avr:uno` with installed Arduino CLI 1.5.1, Arduino AVR Boards 1.8.8 and AVR GCC package `7.3.0-atmel3.6.1-arduino7`. The installed Arduino library folder contains:

| Library | Installed version |
| --- | --- |
| Adafruit GFX Library | 1.12.6 |
| Adafruit SSD1306 | 2.5.17 |
| Adafruit MCP23017 Arduino Library | 2.3.2 |
| Adafruit BusIO | 1.17.4 |

Windows failed to launch `avr-g++.exe` with an **Illegal System DLL Relocation** error. The target compilation did not complete and produced no verified new HEX. Presence of the libraries is not a successful library integration check.

## Runtime limitations

No Proteus or physical-hardware run was performed. Floor position is simulated by time increments, and doors are represented by a state. The analog load-to-display mapping is uncalibrated. Button edge handling is not mechanical debounce. The prepared logic has no physical floor/door feedback, overload shutdown or emergency/interlock handling.

The next meaningful check is an Uno build in a functioning AVR toolchain, followed by circuit connectivity review and a Proteus run using the resulting HEX. Repeat the request and turnaround scenarios against the real simulated peripherals and verify OLED and PWM behavior.
