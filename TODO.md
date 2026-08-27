# 
# 2026-07-27
#
* Add nopnop's DS3231 component.                                                    (done)
* Add ESP-IDF example component for the sd_card.                                    (done)

* Add basic drawing of the ECG monitoring flow to the README.md.                    (done)

* Using the ESP-IDF sdmmc component, fill out the rest of the SD Card data collection handling.
 
* Add the I2C initialization and monitoring, along with controlling I2C timeout parameters, including clock stretching.

* We will eventually need the esp-dsp component particularly if we want to have some ECG analysis done by the ESP32. The basic QRS detection algorithm, the Pan-Tompkins algorithm, requires several DSP functions to be called for each digitized signal sample. For that, we can use much from that collection of DSP techniques. These are optimized for the ESP32 architecture.

* Check in to Github

# 2026-08-09
* I switched over to an older IDF version because of difficulties with using the ESP-IDF Installation Manager. Bad experience with EIM, and I used the older IDF installation installers - using v5.2.7 offline installer 
* Scratch the ESP-IDF installation instructions from the 2026-07-27 text above, since it was problematic, and I recommend using the older and more stable IDF installers.
* Will use the esp-idf-lib (UncleRus) version of ads111x component. Will adjust the calls to use the driver/i2c_master.h
* In order to use all four ADS1115 channels, must adjust the ADC reading process, as per AI suggestions:
    * Cycle Multiplexer & Read:Write the MUX setting for Channel 0 (ADS111X_MUX_0_GND).
    * Read the conversion result over I2C.
    * Switch the MUX setting to Channel 1 (ADS111X_MUX_1_GND), repeat for Channels 2 and 3 sequentially.
* NOTE: Account for Latency: Keep in mind that switching channels sequentially divides your effective sample rate per channel by 4 (e.g., at 860 SPS, each individual channel updates at roughly 215 Hz). (I.e., 860/4 = 215)

# 2026-08-11
* One thing that I need to decide about, is : could this project also be used in a school project of some type? Possible target audiences being biomedical techicians and engineers, certainly medical students, but possibly even ambituous secondary school students (STEM). This project could then be used as a lab teaching guide, or reference source. 
* Since the Tompkins "Biomedical Digital Signal Processing" was heavily used in some of the implementation details present in the project and source code, it might be useful to recommend this here. The intent for the project here is to detail the choices made, based on available cheap but effective parts (the parts can be acquired from inexpensively from Amazon, AliBaba, TEMU, etc.). 

# 2026-08-20
* There have been several updates to the various CMakeLists.txt files, using the ESP-IDF CmakeLists.txt Editor.
* Create basic fritzing diagram(s) of all of the components:
    * ESP32S3-supermini board (the microcontroller)
    * ADS1115 ADC board. This chip has 4 channels of 16-bit ADC capture. The chip can capture up to 860 samples per second, and with 4 channels, each channel can capture 215 SPS (mentioned above). This gives more than 200 Hz for each ECG trace, which would that requirement.
    * Four AD8232 Heart monitor boards. The datasheet for the AD8232 has all of the pertinent information on the filter handling done of the baord to insure adequate noise processing for a clean signal.
    * DS3231 RTC board, for timestamping of ECG traces.
    * MicroSD Card board, that interfaces using SDMMC, for recording the ECG signals captured. The MicroSD ranges from 8GB to 32GB.
    * Power handling: LIPO-2000mah, MT3608 DC convertor, TP4056
* Create paper template using Fritzing's PCB editor, with PCB having the same dimensions as the Phenolic sheets I ordered from Amazon. The Phenolic sheet looks like I can comfortably fit the components needed for the 24/7 ECG continuous monitoring system.
* Created documents based on queries to AI engines (Google, mostly) regarding both hardware and software designs for the current ESP32 ECG system specification, especially circuit design. There is work to be done for the ECG monitor power support.
* build circuit for interfacing the TP4056 (?) charger to the LIPO battery, battery supply to the DC convertor MT3608 (see p.7 of first circuit design document in docs folder). THIS is for Proof-of-concept - this may change,
* build 4-bit SDMCC mode wiring circuit between the MicroSD adapter board and the ESP32S3 super mini board. See page 7 of above mentioned document.
* There are several SW design pieces in the AI queried documents - need to outline them, and describe sequence of actions for implementation.

# 2026-08-25
* I just restarted the project as ESP32-ECGMonitor, partly to emphasize that this targets the ESP32 family, and also to sever development ties to Windows development, and just make it Linux (Ubuntu in specific).
* Using the hello_world example because it would be good to let the user know the chip info.
