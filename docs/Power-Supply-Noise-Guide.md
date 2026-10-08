# Power-Supply Noise for Makers — A Practical Guide

*Distilled from real debugging on the ESP32-ECGMonitor project (ADS1115 + AD8232 ECG front end,
DS3231 RTC, MicroSD logging). Written so other experimenters can skip the "curve balls" and
diagnose power noise with confidence.*

---

## 0. The one idea that prevents most confusion

**Know the frequency of your noise before you reach for a fix.**

Two completely different problems are constantly confused:

| Problem | Frequency | Typical cause | Right fix |
|---|---|---|---|
| Mains "hum" | **50 / 60 Hz** (and harmonics) | ground loop, Y-capacitor leakage | **battery or linear supply**, break the loop |
| Switching "whine"/"hash" | **~100 kHz – several MHz** | boost/buck converter switching | **decoupling caps (+ LC filter)** |

An LC filter tuned for a switching converter does **nothing** for 60 Hz hum. A battery
does **nothing** for a boost converter's own switching ripple. If you don't separate these
two, you'll apply the right tool to the wrong problem — which is exactly how a technically
correct AI answer can still mislead you.

**Rule:** measure first (scope or FFT of the signal). 60 Hz = mains problem. MHz = converter problem.

---

## 1. 60 Hz hum — the ground-loop story

### What's actually happening

Most wall adapters and USB hubs are **switch-mode supplies** with a small **"Y-capacitor"**
between the mains (primary) side and the low-voltage (secondary) side. That capacitor exists
to pass EMI regulations, but it lets a tiny 50/60 Hz current (~µA) leak onto the output ground.

When you then measure a tiny analog signal (an ECG is ~1 mV) referenced to a body that is itself
capacitively coupled to mains, that leakage current flows through your circuit's ground and
appears as 60 Hz in the signal. This is a **ground loop**: two ground paths at slightly
different potentials.

### The fixes, in order of effectiveness

1. **Battery power** — breaks the mains earth reference entirely. This is *the* fix; it
   eliminates the Y-cap leakage path and the loop.
2. **Linear supply** (transformer + regulator) — no Y-cap switching noise, but still mains-referenced.
3. **Isolation** (isolated DC-DC, or USB isolator) — breaks the loop electrically.
4. For ECG specifically: keep the **right-leg drive (RLD)** electrode connected (it actively
   cancels common-mode mains), twist the electrode leads together, and keep them away from
   power cables.

### What does NOT work

- **LC filters / bigger capacitors** — 60 Hz is far below any practical LC corner frequency.
  Filtering 60 Hz on a power rail needs enormous L and C and isn't how you fix it.
- **More decoupling** — decoupling is for transients, not for a fundamental-frequency ground loop.

---

## 2. Switching noise — the converter story

### Why battery power introduces a *new* noise source

A 3.7 V Li-ion cell can't feed a 5 V board directly, so you add a **boost converter**
(e.g., MT3608). The boost switches an inductor at ~1.2 MHz to pump the voltage up. That
switching action injects ~1.2 MHz ripple (plus harmonics) onto the output rail.

This is a *different* problem from 60 Hz — and it's the one an LC filter is actually for.

### How much do you care?

In practice, several things already reject a lot of this for you:

- The board's **onboard LDO** (e.g., the ESP32-S3 Super Mini's 3.3 V regulator) filters the 5 V rail.
- A **delta-sigma ADC** (ADS1115) has an internal digital filter that rejects far out-of-band noise.
- The ECG bandwidth of interest is ~0.05–150 Hz; 1.2 MHz is ~10,000× above it.

So: **start with capacitors, add an inductor only if you actually see switching noise.**

### The capacitor-only approach (do this first)

At the boost output — or better, at the load's power pins:

- **100 µF electrolytic** — bulk, absorbs low-frequency ripple and load steps.
- **100 nF ceramic** — placed as close as possible to the load; handles the MHz spikes
  (electrolytics have too much ESR/ESL to do this).

This is usually enough for a low-power data logger.

---

## 3. The LC filter, done correctly

A series inductor + shunt capacitor is a 2nd-order low-pass:

```
        ── L (10–22 µH) ──●────► to load
                          │
                     C 100 µF
                     (and 100 nF close to load)
                          │
                          ▼
                         GND
```

- Corner frequency: **f_c = 1 / (2π√(L·C))**
  - L = 10 µH, C = 100 µF → **≈ 5 kHz**
  - L = 22 µH, C = 100 µF → **≈ 3.4 kHz**
- Above f_c, attenuation is −40 dB/decade, so 1.2 MHz ripple is knocked down hard.

### The gotchas (this is where makers get burned)

1. **Resonance / Q.** An undamped LC filter can *ring and amplify* noise near f_c.
   Fix: add a small series resistor (0.1–1 Ω) with the electrolytic, or rely on the
   electrolytic's own ESR (which is usually enough).
2. **Inductor saturation.** The inductor must carry the full DC load current without
   saturating. Pick one rated ≥ your peak current (≥ 1 A is a safe default for small boards).
   If it saturates, its inductance collapses and the filter stops filtering.
3. **Transient droop.** A series inductor slows the supply's response to sudden current
   spikes (e.g., Wi-Fi TX bursts). If your load has big transients, the caps carry the
   transient and the L can cause brief droop. (If Wi-Fi is off during critical sampling —
   as in this project — the load is steady and this is a non-issue.)
4. **Layout.** Keep the L–C–GND loop tight and the ceramic cap closest to the load.

### When do you need the full LC filter?

Only if the capacitor-only approach still shows switching noise in the actual signal.
For most small data loggers you won't need it.

---

## 4. General decoupling practice (applies everywhere)

- 100 nF ceramic at **every** IC's power pin, as close as physically possible.
- A bulk cap (10–100 µF) near high-current consumers (SD card, Wi-Fi module).
- **Star ground**: bring grounds to one point; don't daisy-chain analog and digital returns.
- Keep switching converters **physically away** from the analog front end and its leads.

---

## 5. The meta-lesson: how AI (and datasheets) mislead

The DeepSeek exchange that prompted this guide was **technically correct and still misleading**:
it answered *"does an LC filter fix 60 Hz?"* (correctly: no) without the context that the
filter had been suggested for a **different problem** (boost switching noise). A reader could
walk away thinking "LC filters are useless" when the truth is "right tool, wrong job."

How to protect yourself:

1. **Always state the symptom and the frequency range** when asking for advice:
   *"I see 60 Hz hum on my ECG"* is a different question from
   *"I'm adding a 1.2 MHz boost converter and want to know if I should filter its output."*
2. **Ask "what is this fix *for*?"** — a component's purpose (filter switching noise vs.
   break a ground loop) tells you whether it matches your symptom.
3. **Trust the physics, not the prose.** f_c = 1/(2π√(L·C)) will tell you in five seconds
   whether a filter can possibly touch your noise frequency.
4. **Measure.** One FFT/scope capture settles a week of arguing.

---

## 6. Quick decision checklist

- [ ] What frequency is the noise? (scope / FFT)
- [ ] 50/60 Hz → it's a ground/mains problem. Try battery or a linear supply. **Do not** add an LC filter for this.
- [ ] MHz-range → it's a converter. Add 100 µF + 100 nF first; add an LC filter only if needed.
- [ ] If you add an LC filter: is the inductor current-rated and non-saturating? Did you damp the resonance?
- [ ] Is the ceramic cap as close to the load as possible?
- [ ] Is the analog front end physically separated from switching converters?
- [ ] Single ground point (star ground)?
- [ ] Did I state my symptom *and* its frequency when asking for advice?

---

## Glossary

- **Ground loop** — two current paths to ground at different potentials; mains hum flows through it.
- **Y-capacitor** — safety capacitor in a switch-mode supply bridging mains to output; leaks µA of 50/60 Hz.
- **ESR / ESL** — equivalent series resistance / inductance of a capacitor; high ESR/ESL makes
  electrolytics useless at high frequency, which is why you pair them with ceramics.
- **Boost converter** — steps voltage up by switching an inductor; switching freq is typically
  hundreds of kHz to a few MHz.
- **LDO** — low-dropout linear regulator; filters some high-frequency noise but passes low-frequency ripple.
- **fc (corner frequency)** — frequency above which a filter attenuates; f_c = 1/(2π√(L·C)) for an LC filter.
- **Quiescent current** — the current a converter/regulator draws even with no load; matters for battery life.
