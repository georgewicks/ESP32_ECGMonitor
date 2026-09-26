# Digital Signal Processing for the ECG Monitor System

The DSP domain is of particular benefit for the analysis of the ECG recording. We make use of it here for several items:
* In order to determine the period of the QRS wave, we have to detect the QRS from the acquired ECG. We will implement the QRS Detection algorithm from Pan and Tompkins on the ESP32 platform, which uses a series of DSP operations:
    * Band-pass filtering: A Low-pass filter and then a High-pass filter are applied to the raw ECG data. This will improve the signal-to-noise ratio (SNR).
    * Next, a derivative filter is applied to that result. As you can recall from calculus, a derivative of a function can determine the slope, and in this case, it helps in finding the slope of the QRS.
    * Squaring is applied next, which will enhance the QRS peaks.
    * Finally, moving-window integration is applied.
    The paper by Pan and Tompkins, "A Real-Time QRS Detection Algorithm", describes all of these operations in detail. [^1]
* I am using Espressif's ESP-DSP library to optimize the DSP operations on the ESP32 architecture [^2]. There are several modules and examples within this library, and they are quite readable. The most current version of the library is avaialable on GitHub [^3]
* There are also methods for the ST-Segment, a subject near and dear to my heart. [^4][^5]

* The component folder named "QRSDetect" contains AI recommended C functions that implement the various filter methods that are integral to the Pan and Tompkins QRS Detection algorithm.
    * Google AI recommendations:
        * components/QRSDetect/G_AI_REC_Deriv.c:        Derivative filter (both integer & floating point)
        * components/QRSDetect/G_AI_REC_HP_fpver.c      High-pass filter - floating point ver
        * components/QRSDetect/G_AI_REC_HP_intver.c     High-pass filter - integer ver
        * components/QRSDetect/G_AI_REC_LP_fpver.c      Low-pass filter - floating point ver
        * components/QRSDetect/G_AI_REC_LP_intver.c     Low-pass filter - integer ver
        * components/QRSDetect/G_AI_REC_MWI.c           Moving Window Integrator (both integer & floating point)
    
# References

The "Biomedical Digital Signal Processing" textbook is the most thorough and relevant information on DSP, particularly on the Biomedical application of DSP technology. 

Some basic good overview material on Electrocardiology:
* Wikipedia Electrocardiography: https://en.wikipedia.org/wiki/Electrocardiography
* The Invention of Electrocardiography Machine: https://pmc.ncbi.nlm.nih.gov/articles/PMC6881865/

[^1]: Pan, Jiapu; Tompkins, Willis J. (March 1985). "A Real-Time QRS Detection Algorithm". IEEE Transactions on Biomedical Engineering. BME-32 (3): 230–236. doi:10.1109/TBME.1985.325532. PMID 3997178.
[^2]: https://docs.espressif.com/projects/esp-dsp/en/latest/esp32/index.html
[^3]: https://github.com/espressif/esp-dsp
[^4]: https://litfl.com/st-segment-ecg-library/
[^5]: https://www.saem.org/about-saem/academies-interest-groups-affiliates2/cdem/for-students/online-education/m3-curriculum/group-electrocardiogram-(ecg)-rhythm-recognition/stemi

