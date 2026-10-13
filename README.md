# esp32-monitoring-node

[![CI](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml/badge.svg)](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml)

ESP32 monitoring node: temperature and humidity (DHT11), OLED, RGB status LED,
metrics over TCP in `metric-protocol` v2. Evolved from
[esp32-lab](https://github.com/poberezhnyipavlo/esp32-lab) (stages 07 and 13).

> Status: 14.1 — project skeleton, tooling and CI. Sensors arrive in 14.2.

## Run locally
...venv, scripts/format.sh, scripts/tidy.sh, pio test -e native, pio run, wokwi-cli...

## CI
| Job | What it checks | Catches |
|---|---|---|
| test | 33 unit tests on Ubuntu (libstdc++) | logic bugs, missing includes (libc++ vs libstdc++) |
| format | clang-format 21.1.8 | style drift |
| tidy | clang-tidy 21.1.6 on lib/ and test/ | ... |
| build | esp32dev + wokwi builds, Wokwi scenario | firmware does not compile / does not start |

What CI does NOT check: src/ (Arduino code) is not analysed by clang-tidy;
real hardware is tested manually.

## Pinned versions
platform 55.03.312-1 (Arduino core 3.3.12), PlatformIO 6.2.0, clang-format 21.1.8,
clang-tidy 21.1.6, wokwi-cli 0.27.1, GitHub runner ubuntu-24.04.

## Sizes
Flash ..., RAM ... (same as esp32-lab/13 before changes).
