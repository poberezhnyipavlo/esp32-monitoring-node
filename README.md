# esp32-monitoring-node

[![CI](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml/badge.svg)](https://github.com/poberezhnyipavlo/esp32-monitoring-node/actions/workflows/ci.yml)

ESP32 monitoring node: temperature and humidity (DHT11) sent over TCP in
[`metric-protocol`](https://github.com/poberezhnyipavlo/metric-protocol) v2, with an RGB status LED;
an OLED display is planned.
Evolved from [esp32-lab](https://github.com/poberezhnyipavlo/esp32-lab) (stages 07 and 13).

Metrics: `temperature_c`, `humidity_pct` every 2 s. Samples wait in a queue while the network
or the receiver is down and are sent oldest first with their age (`age_ms`).

> Status: 14.2 — DHT11 readings sent over TCP; event bus and temperature alerts next.
> Status: 14.3 — RGB status LED done; OLED next.

  ## Hardware

  | Part | Pin | Notes |
  |---|---|---|
  | DHT11 module (KY-015) | `S` → D4, middle → 3V3, `−` → GND | |
  | RGB LED, common cathode | R → D18, G → D19, B → D23, common → GND | 220 Ω resistor on each colour; HIGH = on |

  One resistor per colour, not one on the common leg: red needs ~2 V and green/blue ~3 V, so with a
  shared resistor red would take almost all the current whenever two colours are on.

  ### Status LED

  Highest priority first:

  | Colour | Meaning |
  |---|---|
  | red, steady | sensor not working (no successful reading for 10 s) |
  | blue | starting, no reading yet |
  | red, blinking | temperature out of range (18…27 °C, 0.5 °C hysteresis) |
  | yellow | no Wi-Fi or no receiver |
  | green | all good |

  A dead sensor beats a temperature alert: the last known level is stale, so blinking "too hot" would
  be a lie. Yellow is red + green; with plain on/off it looks yellow-green (the green die is brighter).

  ### Display

  <img src="docs/wokwi-oled.png" width="384" alt="OLED in Wokwi">

      24.3C 41%          climate, last known values (large text)
      OK                 OK / STARTING / TOO HOT / TOO COLD / SENSOR FAIL 12s
      WiFi:ok TCP:no     network

  What to show is decided by `buildScreen()` in `lib/Screen` (pure function, unit-tested on the host);
  the `Oled` driver only draws it. The screen is rebuilt every 250 ms but sent over I2C only when the
  text changes, so the `SENSOR FAIL` counter redraws once a second and a steady screen not at all.
  The node runs without a display: if nothing answers on 0x3C/0x3D the log says `[app] oled not found`.

  The screenshot is from Wokwi, where no receiver listens, hence `TCP:no`.

## Run locally

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements-dev.txt          # pinned clang-format, clang-tidy, PlatformIO

pio test -e native                           # 39 unit tests on the host
scripts/format.sh                            # check formatting (--fix to rewrite)
scripts/tidy.sh                              # static analysis of lib/ and test/

cp include/secrets.example.h include/secrets.h   # then fill in Wi-Fi and receiver
pio run                                      # firmware for the board

pio run -e wokwi && wokwi-cli --timeout 45000 --scenario test.scenario.yaml   # needs WOKWI_CLI_TOKEN
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

At the blue, red and yellow states the scenario also checks the RGB pins (`expect-pin` on 18/19/23),
so swapped colour pins in the firmware fail the run.

The scenario also waits for `[app] oled found`; `diagram.json` has a `board-ssd1306` on 21/22.

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
| Adafruit SSD1306 / GFX / BusIO | 2.5.17 / 1.12.6 / 1.17.4 |

## Sizes

`esp32dev`: Flash 967 712 bytes (73.8 %), RAM 73 328 bytes (22.4 %).
The OLED libraries add about 44 KB of flash.
Stack of the `ui` task: 6144 bytes, at least 3836 left free on hardware (with 4096 only 1932 were left,
too close to the canary after drawing was added).
