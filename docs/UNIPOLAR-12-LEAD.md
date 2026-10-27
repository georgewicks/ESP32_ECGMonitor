# Unipolar (Precordial) 12-Lead ECG — Corrected Design Notes

This document is a **corrected and project-specific** reference for extending
ESP32-ECGMonitor from a single AD8232 bipolar lead to a standard 12-lead ECG.
It distills and fixes the Google AI advice recorded in
`docs/Creating a Unipolar ECG System.odt`, which is directionally correct but
contains several technical errors (see [§9](#9-corrections-to-the-google-ai-notes)).

> **Prototype status:** this is development hardware, not a certified medical
> device. Battery power and no-USB-while-attached are mandatory (see §7).

---

## TL;DR

* Precordial leads V1–V6 are **unipolar**: each measures a chest electrode
  against **Wilson's Central Terminal (WCT)**, not against another limb.
* WCT = (RA + LA + LL) / 3, built with **three matched resistors**.
* A full 12-lead needs **8 AD8232 boards** (Lead I, Lead II, and V1–V6) and
  **4 ADS1115 ADCs** (8 single-ended channels). The Google note's "7 boards"
  is wrong — it omits Lead II, without which aVR/aVL/aVF cannot be derived.
* Keep the existing **interrupt-driven, single-shot RDY** ADS1115 firmware; do
  **not** adopt the note's continuous-mode polling sample.

---

## 1. The concept: WCT and unipolar leads

A standard 12-lead ECG is really 8 measured signals + 4 derived signals:

| Group | Leads | How obtained |
|---|---|---|
| Bipolar limb | I, II | measured directly (2 AD8232) |
| Derived limb | III, aVR, aVL, aVF | computed from I and II |
| Precordial (unipolar) | V1–V6 | measured against WCT (6 AD8232) |

**WCT (Wilson's Central Terminal)** is a synthetic "zero" reference formed by
averaging the three limb potentials:

```
WCT = (RA + LA + LL) / 3
```

Each chest lead is the voltage between a chest electrode and this average:
`Vx = Vx_electrode − WCT`.

---

## 2. Hardware bill of materials (corrected)

| Qty | Part | Role |
|---|---|---|
| 8 | AD8232 board | Lead I, Lead II, V1, V2, V3, V4, V5, V6 |
| 4 | ADS1115 ADC | 8 single-ended channels (2 per chip) |
| 1 | ESP32-S3 (Super Mini or WROOM) | I2C master, capture, storage |
| 10 | electrodes | RA, LA, LL, RL, V1–V6 |
| 3 | matched resistors (100 kΩ, 0.1%) | WCT averaging network |
| 1 | RTC (DS3231) + MicroSD | timestamps + storage (existing) |
| 1 | PCF8574 (optional) | leads-off monitoring (existing plan) |

**Why 8 AD8232, not 7:** Lead I gives `LA − RA` and Lead II gives `LL − RA`.
From those two you can compute III, aVR, aVL, aVF. The Google note assumed one
limb lead ("Lead I") plus six chest leads — that yields only 7 leads and cannot
produce the aV family.

---

## 3. Wiring

### 3.1 WCT averaging network

```
RA ───[100k 0.1%]───┐
LA ───[100k 0.1%]───┼─── WCT node
LL ───[100k 0.1%]───┘
```

* Resistor **matching** matters more than the absolute value (equal values give
  a true average). 100 kΩ keeps the node light on the body; 10–100 kΩ is fine.
* **Robustness:** the WCT node is high-impedance and feeds six −IN inputs.
  For a prototype, direct connection works (AD8232 inputs are high-impedance).
  For a robust build, add a **unity-gain op-amp buffer** per chest board (or at
  least one low-offset buffer at the WCT node) so the six loads don't pull the
  average.

### 3.2 AD8232 connections

| Board | +IN | −IN | RLD |
|---|---|---|---|
| Limb #1 (Lead I) | LA | RA | **master** → RL electrode |
| Limb #2 (Lead II) | LL | RA | disconnected |
| Chest V1 | V1 electrode | WCT | disconnected |
| Chest V2 | V2 electrode | WCT | disconnected |
| Chest V3 | V3 electrode | WCT | disconnected |
| Chest V4 | V4 electrode | WCT | disconnected |
| Chest V5 | V5 electrode | WCT | disconnected |
| Chest V6 | V6 electrode | WCT | disconnected |

**Single-RLD rule:** exactly one AD8232 (the Lead I board) drives the right-leg
electrode. Every other board's RLD pin stays floating. Multiple active RLD
drivers fight each other and cause oscillation / massive artifact.

### 3.3 ADS1115 channel map (4 chips, 8 channels)

| ADS1115 | ADDR | Address | AIN0 | AIN1 |
|---|---|---|---|---|
| #1 | GND | 0x48 | Lead I | Lead II |
| #2 | VDD | 0x49 | V1 | V2 |
| #3 | SDA | 0x4A | V3 | V4 |
| #4 | SCL | 0x4B | V5 | V6 |

* Each chip runs your existing **single-shot, ALERT/RDY-interrupt, 2-channel
  mux** loop. Expect ~300–357 SPS per channel (bus contention grows with four
  chips). This is adequate for diagnostic ECG (250–500 Hz).
* Chips #3/#4 need two more ALERT/RDY GPIOs (currently GPIO21 and GPIO7 are
  used). Verify two free GPIOs on the Super Mini; if none, reassign pins or
  route leads-off through the PCF8574 and reclaim GPIOs.

---

## 4. Electrode placement (standard)

| Electrode | Location |
|---|---|
| RA | right arm |
| LA | left arm |
| LL | left leg |
| RL | right leg (driven, from the master AD8232 RLD) |
| V1 | 4th intercostal space, right sternal border |
| V2 | 4th intercostal space, left sternal border |
| V3 | midway between V2 and V4 |
| V4 | 5th intercostal space, midclavicular line |
| V5 | horizontal with V4, anterior axillary line |
| V6 | horizontal with V4, mid-axillary line |

---

## 5. Lead derivation (software)

Once firmware has **I** and **II** (plus V1–V6), compute the rest:

```
III = II − I
aVR = −(I + II) / 2
aVL =  I − II / 2
aVF =  II − I / 2
```

Do this in **floating point** for display/analysis, but note the ISHNE file
stores the **raw int16** samples; the derived aV leads are floats, so quantize
them back to int16 before writing (see §6 resolution note).

---

## 6. ISHNE output

The ISHNE standard stores leads in a fixed order (see `components/ECG-ISHNE/ISHNE.h`):

```
I, II, III, aVR, aVL, aVF, V1, V2, V3, V4, V5, V6   (codes 5..16)
```

So set `nleads = 12` and write 12 interleaved `int16` per sample. Firmware must:

1. Sample I, II, V1–V6.
2. Derive III, aVR, aVL, aVF.
3. Write all 12 in the order above.

**Resolution caveat (carried from the current writer):** the ISHNE
`resolution` field is a signed 16-bit **nV/LSB** value (max 32767). At
±2.048 V the ADS1115 LSB is 62500 nV, which overflows. Options:

* Store 31250 nV/LSB (valid file, amplitude 0.5× — current workaround), or
* Switch to **differential mode at max PGA (±0.256 V → 7812 nV/LSB)** once the
  front-end moves to a differential/WCT-referenced input, which both fits the
  field and removes DC offset.

---

## 7. Safety (mandatory)

* **Battery power only** while electrodes are attached.
* **Never connect USB to a wall-powered computer/laptop** with the patient
  attached — mains ground loop through the body.
* The AD8232 has **no defibrillation-grade isolation**. This is a prototype;
  not for use near defibrillators or as a clinical device.
* Every board's leads-off detection (LO+/LO−) should be wired (PCF8574) so a
  lost electrode is detected — especially the RL electrode, which LO+/LO− do
  **not** cover and which causes rail saturation when it drops.

---

## 8. Phased roadmap

1. **Prove WCT with one V-lead.** Build the 3-resistor WCT, add one chest
   AD8232 (V1 or V2) against WCT, RLD disconnected, and record it beside the
   existing Lead I. Confirm a clean, correctly-oriented QRS.
2. **Scale to 8 boards / 4 ADCs.** Add Lead II + the remaining chest leads,
   two more ADS1115 chips (0x4A/0x4B) and their ALERT GPIOs.
3. **Firmware.** Extend the dual-chip writer to four chips, add the §5
   derivations, write 12-lead ISHNE.
4. **Robustness.** Buffer the WCT, battery power, twisted/shielded leads,
   3M electrodes, leads-off via PCF8574.

---

## 9. Corrections to the Google AI notes

| Google AI said | Correction |
|---|---|
| 7 AD8232 for "full V1–V6" | 8 boards — add Lead II, or aVR/aVL/aVF are impossible |
| Poll `ads111x_get_voltage()` in continuous mode | Reads stale data; keep the existing interrupt/single-shot RDY loop |
| Gain ±4.096 V | ±2.048 V is finer (62.5 µV/LSB); either works |
| ESP-IDF v6.0 uses **Picolibc** | False — ESP-IDF uses newlib |
| Pull `esp-idf-lib/ads111x` from the registry | Conflicts with the local modified `ads111x` component |
| (omitted) lead derivation | III/aVR/aVL/aVF must be computed from I and II |
