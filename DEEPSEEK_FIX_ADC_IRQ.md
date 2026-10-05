# ADS1115 ADC Capture / ALERT-IRQ Fix Summary

## Symptom
The ESP32 firmware was failing to capture ECG samples from the ADS1115.
The ALERT/RDY interrupt either never fired or fired only once, so the
acquisition task received "nothing, or just one" sample and the status line
showed `ISR count = 0` with repeated 2-second timeout warnings.

## Root Cause
The ADS1115 config register value (`ads_mux_config[]`) did not match the
intended "single-shot + active-low + conversion-ready interrupt" design:

- The original values (`0xC4E8` …) decoded to **continuous conversion mode**,
  **active-high** ALERT polarity, and **comparator enabled** (`COMP_QUE=00`).
  With the traditional comparator and thresholds that an ECG signal never
  crosses, the ALERT/RDY pin never asserted, so the falling-edge ISR
  (`GPIO_INTR_NEGEDGE`) never fired.

- An intermediate fix used `COMP_QUE=11`, which the datasheet says
  *"disables the comparator function and puts the ALERT/RDY pin into a high
  state"* — holding the pin permanently high and producing **zero** edges.

## The Correct Conversion-Ready (RDY) Configuration
Per the ADS1115 datasheet, the ALERT/RDY pin becomes a **conversion-ready**
output when **all** of the following hold:

| Field / Register | Required value | Notes |
|---|---|---|
| `Hi_thresh` | `0x8000` | MSB = 1 enables RDY mode |
| `Lo_thresh` | `0x0000` | MSB = 0 enables RDY mode |
| `COMP_QUE` | `00` | must **not** be `11`, or the pin is disabled |
| `COMP_POL` | `0` | active low → pin asserts low at end of conversion |
| `MODE` | `1` | single-shot (pin mirrors OS bit) |
| ALERT pin | pull-up enabled | ALERT/RDY is open-drain and requires a pull-up |

In single-shot mode with `COMP_POL=0`, the pin is deasserted (high) while a
conversion runs and asserts **low** when the conversion completes — producing
the falling edge the `NEGEDGE` ISR waits for.

### Correct config values (per channel, single-shot, 860 SPS, active-low)

| Channel | Value |
|---|---|
| Ch 0 (AIN0/GND) | `0xC5E0` |
| Ch 1 (AIN1/GND) | `0xD5E0` |
| Ch 2 (AIN2/GND) | `0xE5E0` |
| Ch 3 (AIN3/GND) | `0xF5E0` |

Bit decode of `0xC5E0`:
`OS=1`, `MUX=100 (AIN0/GND)`, `PGA=010 (±2.048 V)`, `MODE=1 (single-shot)`,
`DR=111 (860 SPS)`, `COMP_MODE=0`, `COMP_POL=0 (active low)`, `COMP_LAT=0`,
`COMP_QUE=00 (pin enabled)`.

## Changes Made

### `main/main.c`
- `ads_mux_config[]` corrected to `0xC5E0` / `0xD5E0` / `0xE5E0` / `0xF5E0`.
- Removed per-sample `ESP_LOGI` calls from the acquisition loop (they would
  throttle the capture rate at UART speeds).
- Replaced `xSemaphoreTake(..., portMAX_DELAY)` with a real
  `pdMS_TO_TICKS(2000)` timeout; the timeout branch now re-primes a conversion
  so the loop can recover instead of hanging silently.
- Reordered the acquisition loop to write the next channel's config **before**
  reading the result, so the I2C read overlaps the next conversion and the
  single-shot idle gap is minimized.
- Removed the per-interrupt `gpio_set_level(MY_BLINK_PIN, ...)` from the ISR.
- Enabled the internal pull-up on `ADS_ALERT_GPIO` (open-drain active-low).
- Made `isr_count` `volatile`; removed dead `delayT` and unused `f_val` code.

### `components/ads111x/ads111x.c`
- Demoted per-call `ESP_LOGI` in `read_conf_bits`, `write_conf_bits`,
  `ads111x_start_conversion`, `ads111x_get_value`, and `ads101x_get_value` to
  `ESP_LOGD` so they cannot throttle capture.

### `main/I2C_Handling.c`
- `InitADS1115()` high threshold changed from `0x7FFF` to `0x8000` (MSB = 1,
  required to enable RDY mode).
- ADS1115 I2C clock speed raised from 100 kHz to 1 MHz (fast-mode-plus).

## Verification
- Firmware builds cleanly (`ESP32-ECGMonitor.bin` generated).
- On hardware: `ISR count` and `ADC count` now climb together.
- Measured rate was ~640 SPS total (~160 SPS/channel) in single-shot 4-channel
  muxing mode; the write-first/read-second reorder and 1 MHz I2C are expected
  to raise this toward ~750–800 SPS. The ADS1115's 860 SPS figure is the
  continuous-conversion rate for a single channel and is not reachable while
  multiplexing four channels in single-shot mode.

## Optional Follow-up Cleanup (not required for ADC capture)
- Pre-existing compiler warnings: unused `count` in `app_main`, unused
  `ESampCnt`, unused `bcd_to_dec`, unused `buf` in `Init_EEPROM`.
- Dead `#if 0` `InitADS1115` block in `I2C_Handling.c` and the uncompiled
  `main/ADCCapture.c` still contain the old `0xC4E8` / `0x7FFF` values.
