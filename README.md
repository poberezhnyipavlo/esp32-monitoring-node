# esp32-monitoring-node

[![CI](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml/badge.svg)](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml)

ESP32 monitoring node: temperature and humidity (DHT11) sent over TCP in
[`metric-protocol`](https://github.com/poberezhnyipavlo/metric-protocol) v2; OLED and an RGB status LED are planned.
Evolved from [esp32-lab](https://github.com/poberezhnyipavlo/esp32-lab) (stages 07 and 13).

Metrics: `temperature_c`, `humidity_pct` every 2 s. Samples wait in a queue while the network
or the receiver is down and are sent oldest first with their age (`age_ms`).

> Status: 14.2 — DHT11 readings sent over TCP; event bus and temperature alerts next.

## Run locally

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements-dev.txt          # pinned clang-format, clang-tidy, PlatformIO

pio test -e native                           # 39 unit tests on the host
scripts/format.sh                            # check formatting (--fix to rewrite)
scripts/tidy.sh                              # static analysis of lib/ and test/

cp include/secrets.example.h include/secrets.h   # then fill in Wi-Fi and receiver
pio run                                      # firmware for the board

pio run -e wokwi && wokwi-cli --timeout 30000 --scenario test.scenario.yaml   # needs WOKWI_CLI_TOKEN
```

## CI

Five jobs on every push to `main` and every pull request (documentation-only changes are skipped):

| Job | What it checks | Catches |
|---|---|---|
| test | 39 unit tests on Ubuntu (libstdc++) | logic bugs; includes that only work with libc++ on macOS |
| format | clang-format 21.1.8 | style drift |
| tidy | clang-tidy 21.1.6 on `lib/` and `test/` | bug-prone code, naming, magic numbers, missing includes |
| build | `esp32dev` and `wokwi` firmware builds | firmware that does not compile |
| wokwi | Wokwi scenario on the built firmware | firmware that does not start, stops sampling, or ignores a sensor fault |

Every job was proven to fail on purpose (a deliberately broken pull request: one failing test,
one unformatted line, one magic number, one missing secret, one wrong scenario line).

Caching `~/.platformio` cut the `build` job from 3 min 31 s (cold) to about 1.5 min.
Every job has a 15-minute limit, so a test stuck in an endless loop cannot burn the default 6 hours.

**Not covered by CI:** `src/` (Arduino code) is not analysed by clang-tidy, only compiled;
real hardware is tested manually.

### Wokwi scenario

`test.scenario.yaml` runs on the `wokwi` build (DHT22, Wokwi-GUEST): start, Wi-Fi,
a failed TCP connect and a retry (nobody listens on the gateway), DHT readings,
then a sensor fault and recovery.

**Simulated sensor fault.** Wokwi's DHT22 has no scenario controls and a wire cannot be removed
from a running simulation, so `diagram.json` has an extra push button `btn_dht` between the DHT
data line and GND. Released, the circuit matches the real board; pressed (by the scenario), the
data line is held low, the sensor stops answering and the firmware must log `[app] dht read failed`
and keep sampling. This button does not exist on the real hardware.

## Code style

Google C++ Style Guide, enforced by clang-format and clang-tidy, with two deliberate deviations
to match the Arduino ecosystem:

- functions and methods are `camelCase` (`begin()`, `popFront()`), like the Arduino API;
- files are named after the class (`RingBuffer.h`), like `WiFi.h` and `HardwareSerial.h`.

Indentation is 4 spaces; arguments that do not fit go one per line.

## Pinned versions

| Tool | Version |
|---|---|
| pioarduino platform | 55.03.312-1 (Arduino core 3.3.12, ESP-IDF 5.5.5) |
| PlatformIO | 6.2.0 |
| clang-format / clang-tidy | 21.1.8 / 21.1.6 |
| wokwi-cli | 0.27.1 |
| Adafruit DHT sensor library | 1.4.7 |
| GitHub runner | ubuntu-24.04 |

## Sizes

`esp32dev`: Flash 921 712 bytes (70.3 %), RAM 71 680 bytes (21.9 %).
