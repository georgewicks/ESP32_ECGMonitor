# ESP32-ECGMonitor
* This is a low-cost continuous real-time ECG monitor using Espressif's ESP32 architecture, along with their ESP-IDF framework. The bulk of the code is in C, and it is accessible from a GitHub repository. We are using cheap, off-the-shelf components that are easily acquired.

Personally, this design came about because of a very serious health crisis that I went through, namely a heart attack. In April, 2024, I was moving some extremely heavy bags of soil for my garden, when I felt tremendous pressure in both shoulder, and I grew very fatigued. I wound up first in the Emergency Room at Skokie Hospital, where the medical team first stabilized me, and prepared me for transport to Evanston Hospital. Arriving at Evanston Hospital, I was immediately sent to their Cardiac Cath Lab, where I underwent a Cardiac Catheterization procedure, and two stents were inserted into the cardiac arteries. There was more than 90% blockage. I received excellent from the Emergency care team at Skokie Hospital, and then from the Cardiac Care teams at Evanston Hospital. I am forever grateful to the excellent physicians, nurses, and the other clinicians at Endeavor Health.

This was a devastating health event for me, and as I recovered, I was determined to finish an ECG monitoring project I had started a few years prior to this crisis. This event did push me to resurrect this project.

The following is a very rough sketch of the ECG system. NOTE: the drawing reflects the use of an ESP32-DevKit, but that has since been changed to now use a smaller ESP32 board, the ESP32S3-supermini board.

* <img src="./images/ECG-Flow.jpg" alt="ECG Flow Diagram" width="40%">

* A rough sketch to illustrate how the ECG Monitor system works.
    * The electrodes **(0)** are placed on the patient being monitored.
    * The electrode wire bundle is connected to a AD8232 **(1)**, a heart rate front end.
    * The signal data from the AD8232 is attached to the ADS1115 **(2)**, a 16-bit analog-to-digital converter.
    * The digitized data is collected from the I2C serial bus (the wires labeled **(4)** and **(5)**), and processed by the ESP32 DevKit microcontroller **(3)**.
    * The data and analysis is recorded to the MicroSD adapter board **(6)**.
    * The RTC (Real-time clock **(7)**) will add time data that is inserted into the MicroSD adapter **(6)**, thus providing time stamped ECG charting for clinical technicians who are monitoring the data.

* The primary hardware components of the project are:
    - **ESP32 DevKit WROOM-32U**. (Note, I have recently tried the **ESP32S3**, which is smaller, and that works well)
        * <img src="./images/small-ESP32S3-sm.jpg" alt="ESP32S3" width="30%">
    - **(1) ADS1115 Breakout boards. 16-bit ADC, 4 ADC channels**
        * <img src="./images/ads1115" alt="ADS1115" width="30%">
    - **(4 are used for a full 12 electrode setup) AD8232 Physiological Monitor**.
         * <img src="images/ad8232.jpg" alt="AD8232" width="30%">
    - **RTC. Needed for real-time charting updates for time stamping. (DS3231). In addition, the board also has a AT24C32 which will be used as storage for a configuration file**
         * <img src="./images/DS3231.png" alt="DS3231" width="30%">
    - **MicroSD 8GB Adapter - Recording ECG Traces. (SPI Mode)**
         * <img src="./images/microsd-board.JPG" alt="MicroSD board" width="30%">

* The primary software components of the project are:
    * We will be using ESP-IDF, v5.2.7. This will necessitate the use of the I2C_master driver.
    * The ESP-IDF package includes a Real Time Operation System, called FreeRTOS. This provides the framework for orderly handling of the data collection and the various tasks that are undertaken by the microcontroller.
    * The software components for the various devices used by the ECG monitor - the drivers for the ADC chips, the RTC board, the MicroSD adapter.
    * The cardiac data analysis will need to handle QRS detection, which gives the basic heart rate data. The data analysis component may also be expanded to include more features which can be very useful for monitoring patient health.

# Processing flow for ECG signal capture, processing, and storage
![ECGMonitor](images/ECG-capture-n-proc.png)
* This represents the flow for the ECG signal process.
    * On the far left, there is a ADC Capture component that stores the captured signal into a Packet buffer.
    * When the maximum for the packet size is reached, the ECG Process Task is called.
    * This in turn encompasses QRS Detection, which will be handled by the Pan-Tompkins algorithm, which result in the primary metrics, the QRS duration and the R-R interval.
    * Following that, an ST-segment analyzer is called.
    * There may be one or more algorthms called after that (TBD)
    * The recording of the data follows, with a call to generate a timestamp for the packet trace
    * The data is written to the MicroSD card.
    * Optionally, a LCD screen if present and configured can be updated with the new data as well.

# Additional Documentation
* [Power-Supply Noise Guide for Makers](./docs/Power-Supply-Noise-Guide.md) — how to diagnose and fix 60 Hz mains hum vs. switching-regulator noise in this design (battery power, grounding, decoupling, and LC filters).



