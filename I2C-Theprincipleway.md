# I2C - the principle communication between devices in the Monitor
This is an overview and basic reference.
**I2C** (Inter-Integrated Circuit) is a very fast method of communicating between a CPU (microcontroller CPU in this case) and various semiconductor chips over a data line, with another line that supplies a clock signal. The clock signal can be of as a synchronizing method for the transfer of the data. 

Data in a processor is stored in memory, typically in RAM or Flash. In most motherboards in desktop systems, data can be transferred in a parallel method, which very fast, BUT at a cost for the motherboard layout design. Devices in a motherboard must normally use many copper traces on the PCB. This can be quite complex and is costly, since data is transferred in a parallel manner. 

For systems that use I2C, the data is sent in a serial manner. This means that the device takes data in parallel form, and send the data over a serial bus, bit by bit.
(Drawing - TBD)

Data that is manipulated by a processor in several byte sized amounts. A single character, a byte, has 8 bits. 