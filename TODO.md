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

# 2026-08-26
* found that there are 2 different pinouts for the ESP32S3-supermini - very tedious debug, before I realized that the diagram was using was the wrong one. Must remember to always check the VCC (3.3v) output pin everytime when using a new chip - don't rely on the USB power up - check the power pinouts, because then you can confirm I2C (since you need pull-up resistors for the SDA and SCL pins).
* Got the basic foundation for the current ECG monitor setup. Basic chip info, I2C initialization.
* Start the implementation of the AI suggested code. It does make sense. (See docs/AI-assisted-circuit-and-SW-design/CodeSnapshotsFromAI)

# 2026-08-30
* Asked and received good review and debugging from ESP Docs AI Assistant. I was able to get the full path working from ADC capture to writing out the data to the MicroSD card

# 2026-08-31
* must optimise the ADC data stream for ECG analysis: need to expand the adc_sample_t structure so that 215 samples per channel for the ECG trace data.

# 2026-09-01
* Added SNTP time synchronization to the DS3231                    - done, BUT fix ds3231_set_time!
* Must add 10k pull-ups to MISO, MOSI, CLK, and CS lines.          - done
* debug ds3231_set_time
* ECG analysis and processing:
    * Add esp-dsp component for the Pan-Tompkins QRS SW
    * Layout the QRS Detect workflow: from the first DSP operations to the write of the final chart segment for the given sample.
* Look into expanding the adc_sample_t structure to now have 215 16-bit ADC samples per channel, this having a total of 860 16-bit samples per second.
    * Have a dual sample buffers, one to send on to Analysis and Processing stage, the other as the current sampling buffer for the ADC task
    * Make it "ping-pong" scheme, using pointers of the adc sample buffers
    * Using already allocated memory buffers avoids using dynamic memory allocation issues.
    
# 2026-09-02
* Add PCF8574 GPIO extender component: we can use that for handling the Leads Off indication from the AD8232 heart monitor board.

# 2026-09-03
* Had problems with commanding the PCF8574. After a lot of AI assisted debugging, tracked down the problem to be the extra pull-up resistors on the blue breadboard - they're SMD resistors, I'm not going to remove. Ordered PCF8574s in 16 DIP package, since they're strictly just the ICs, there are no extra pull-up resistors to contend with. I wrote a description of the problem, and put it within the docs folder.
* The AI assistant pointed out that there are other boards with potential problems involving bus overloading. Good to be made aware of the limitations that are inherent when using these small boards. Also drives home the point that there should be datasheets available. A lot of the places that you buy these parts from do not provide datasheets, which is certainly aggravating, but just some extra work is needed to be done. 
* Must start building up the basic folder and file structure for the ECG Analysis. Need QRS Detection as one of the most important items, along with the inclusion of ESP-DSP component.
* Flesh out the recording of the ADC sample to the ECG-ISHNE formatted file.
    * Define the ADC sample structure:
        4 channels
        215 16-bit sample   (each channel captures 215 samples, for a grand total of 860 SPS)

# 2026-09-04
* Add more info on Digital Filter design. Put together a workflow description of how engineers design and implement digital filters for signal processing.

# 2026-09-07
* Created Jupyter Book for the ESP32-ECGMonitor
* Need to add sections on:
    * Development of the firmware using the ESP-IDF framework
        * Show installation of ESP-IDF using the EIM
        * Show creation of CMakeLists.txt
            * ADS1115
            * main
            * Project CMakeLists.txt
