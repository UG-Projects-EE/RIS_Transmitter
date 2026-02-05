# RIS_Transmitter    
This repository contains the implementation of a simplified wireless transmitter using a Space-Time Coding Metasurface (STCM). The project uses Arduino UNO Q to achieve real-time beamsteering and signal modulation at a 160 MHz clock speed.
# Overview
The Arduino UNO Q (ABX00162) serves as the control hub for our in-house developed Space-Time Coding Metasurface (STCM). The high-level computational power of its Qualcomm® Dragonwing™ QRB2210 helps determine the spatial phase gradient for beam steering and the time-domain coding sequence for digital modulation in real time. Its STMicroelectronics® STM32U585  (Arm® Cortex®-M33) microcontroller obtains the spatial phase information and the time-domain coding sequence and channels them to 16 columns of the RIS board to enable the required switching of PIN diodes. The two domains communicate via the Arduino Bridge, a software-based Remote Procedure Call (RPC) layer. 
# Project Structure
main.py: Python script running on Debian Linux for codebook index selection for a desired beam angle and digital modulation. <br>
sketch.ino: Arduino sketch running on the STM32 for high-speed switching of PIN diodes using hardware timers. <br>
ris_codebook.h: C++ header containing the $L=4$ binary sequences generated from MATLAB simulations. <br>
(These codes are then run using Arduino App Lab platform) <br>
/hardware: Includes wiring diagrams for connecting the RIS to the JDIGITAL and JANALOG headers.
# Hardware Implementation
Wiring Map from UNO Q to the 16 columns of RIS: <br>
To achieve maximum performance, elements are mapped to Port A and Port B for simultaneous updates. Elements 0–7: Connected to JDIGITAL (Port B). Elements 8–15: Connected to JANALOG (Port A).
# Critical Hardware Notes
Power: Ensure a 5 V at 3 A supply via USB-C. The high-speed switching and Linux MPU require stable current. <br>
Voltage: All MCU GPIOs on JDIGITAL and JANALOG are 3.3 V logic. <br>
Safety: A0 and A1 pins are direct ADC inputs and are NOT 5 V-tolerant. <br>
Timing: The board takes 20-30 seconds to boot Linux. The MCU will start its switching logic as soon as the ris_codebook is loaded and the timer starts.
