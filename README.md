# BendeRadio BT

A Bluetooth speaker in a Bender head, forked from [AlexGyver's BendeRadio](https://alexgyver.ru/benderadio/).

The eyes follow the music, the mouth shows a live spectrum, and a single
rotary encoder handles everything else.

**Current branch: `bluetooth-implementation`** — this holds the latest stable code.

## What's different from upstream

- **Bluetooth audio (A2DP)** replaces Wi-Fi internet radio. Pair your phone and play.
- **Wi-Fi and the station list are gone**, along with the `ESP32-audioI2S` dependency.
- **FFT spectrum analyser** on the mouth — 24 columns, logarithmic band spacing,
  peak dots, and a kick detector that makes the pupils twitch on the beat.
- **Removed `analyz1`**, the scrolling bar visualiser.
- **Brightness capped at 4** (was 16). High brightness draws enough current to
  brown out the board on a weak supply. Raise it back if your power delivery is solid.

## Hardware

| Part | Notes |
|---|---|
| ESP32 DevKit | 30 or 38 pin, Espressif core (not the Arduino Nano ESP32 fork) |
| 5× MAX7219 modules | 3 for the mouth, 2 for the eyes, daisy chained |
| 5× 8×8 LED matrices | common cathode (`1088AW` / `1288AW` / `1388AW`) |
| PCM5102 | I2S DAC |
| PAM8403 | class-D amplifier |
| 2× 40 mm speakers | 4 Ω, full range |
| EC11 rotary encoder | with push button |
| Power supply | 5 V, 2 A minimum |

Plus the analyser input network on GPIO34: a 1 µF coupling cap from the DAC's
left channel, a 10k/10k divider to bias the pin at half rail, and a 1k + 0.01 µF
low-pass right at the pin.

## Controls

| Action | Result |
|---|---|
| Turn | Volume |
| Click | Power on / off |
| Double click | Switch mouth mode (wave ↔ spectrum) |
| Triple click | Calibrate the wave-mode threshold |
| Click + hold + turn | Mouth brightness |
| 2x Click + hold + turn | Eye brightness |

Settings persist in EEPROM.

## Build

PlatformIO, ESP32 core 2.0.x:

```ini
[env:esp32dev]
platform = espressif32@6.9.0
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    https://github.com/GyverLibs/EEManager.git
    https://github.com/GyverLibs/EncButton.git
    https://github.com/GyverLibs/GyverMAX7219.git
    https://github.com/GyverLibs/VolAnalyzer.git
    https://github.com/tierneytim/btAudio.git
    fastled/FastLED@3.6.0
    kosme/arduinoFFT@^2.0.4
```

## Things that cost me time

**`XSMT` on the PCM5102 must be tied to 3.3V.** Out of the box those solder
jumpers are often left open, which parks the chip in soft mute. The DAC powers
up, the LED lights, data arrives, and nothing comes out. Also tie `SCK` to ground
so the internal PLL takes over — the ESP32 doesn't supply MCLK.

**The level shifter might be optional.** BSS138 boards did not work on my end.
Driving the matrices straight from 3.3 V logic is out of spec on paper but worked
reliably in practice. 

**Star the power wiring.** Run separate pairs from the barrel jack to the
amplifier, the matrices, and the board — never daisy chain through the ESP32.
Put 1000 µF right at the amplifier and 470 µF at the matrices. Without this, a
volume spike drags the rail down, the matrices latch into display-test mode, and
the board browns out.

**The analyser input is noise-sensitive.** Keep the wire from the DAC to GPIO34
short and twisted with its own ground, away from the matrix and I²S runs. Verify
the divider actually biases the pin to ~1.65 V with the signal disconnected — a
wrong resistor there will have the analyser running on pickup noise instead of
audio, which looks convincing until you pause the music.

## Credits

Original project and 3D models by [AlexGyver](https://alexgyver.ru/benderadio/).
Bluetooth port based on [im-tortik/BendeRadioBt](https://github.com/im-tortik/BendeRadioBt).
