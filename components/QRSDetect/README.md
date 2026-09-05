# QRS Detection
#   The Pan-Tompkins algorithm is the chief foundation.
* Goal here is to follow the outline of the process, taken directly from the paper [ref]

An analog filter bandlimits the ECG signal at 50 Hz. An ana-
log-to-digital converter (ADC) samples the ECG at a rate of 200
samples/s. The resulting digital signal passes successively
through a sequence of processing steps that includes three
linear digital filters implemented in software. First is an integer-
coefficient bandpass filter composed of cascaded low-pass and
high-pass filters. Its function is noise rejection. Next is a filter
that approximates a derivative. After an amplitude squaring
process, the signal passes through a moving-window integrator.
Adaptive thresholds then discriminate the locations of the QRS
complexes.

* The above is directly taken from "A Real-Time QRS Detection Algorithm", Pan, Tompkins, IEEE TRANSACTIONS ON BIOMEDICAL ENGINEERING, VOL. BME-32, NO. 3, MARCH 1985, page 3, section marked "METHODS".

* First step is the bandpass filter, which is composed of a low-pass filter, followed by a high-pass filter. As stated above, this is to remove noise.

