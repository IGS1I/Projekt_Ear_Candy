# 🎧 Embedded Systems Development (ESD) — MP3 Player

A fully custom MP3 player designed and built from scratch as part of the Embedded Systems Development (ESD) team of INIT's BUILD program at FIU. The project focuses on creating a functional music player capable of storing, managing, and playing audio (.mp3) files while integrating a graphical interface, physical controls, and a dedicated audio subsystem.

This repository documents the complete engineering process—from schematic design and PCB fabrication to firmware development and system integration—providing a hands-on experience in embedded hardware and software design.

---

## 🚀 Project Overview
The goal of this project is to develop a standalone MP3 player powered by an ESP32-WROVER-E-N16R8 microcontroller. The device features:

- A graphical display for browsing the music library, viewing the currently playing track, and showing lyrics.

- A dedicated MP3 decoding chip ([VS1053b](https://www.vlsi.fi/en/products/vs1053.html)) for quality audio playback.

- A custom PCB integrating power management, audio circuitry, storage, and user input.

- Firmware written in C++ to handle playback, UI rendering, storage access, and peripheral communication.

This project emphasizes teamwork, embedded design principles, and iterative development using modern engineering tools.

Quick link to [Programming Section](#-programming) 

---
## 🛠️ Core Features
- MP3 playback using the VS1053b audio decoder
- Song storage via SD card
- Graphical interface on TFT-LCD Color Display
- Physical input controls for navigation, selection, volume, and playback
- Custom PCB with integrated power system and protection circuitry
- Firmware built using ESP-IDF, Docker, CMake, and Ninja

---
## 🧩 Tech Stack
- Firmware Development: ESP-IDF (C++)
- Hardware Design: Altium Designer (schematics + PCB layout)
- Version Control & Collaboration: GitHub, Altium 365

---
## 🔌 Hardware Components
- Microcontroller Unit:
    - [ESP32-WROVER-E-N16R8](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-WROVER-E-N16R8/11613135?s=N4IgTCBcDaIKIGUAKBmMBaA6gJQPIDU5t050A5ARgDZsAOEAXQF8g)
    - [TCA6416ARTWR](https://octopart.com/part/texas-instruments/TCA6416ARTWR?utm_source=altium_a365_bom_portal) - 16-bit I/O Expander
    - [WS2812B-2020](https://octopart.com/part/worldsemi/WS2812B-2020?utm_source=altium_a365_bom_portal) - Smart LED process indicator
- Audio Processing:
    - [VS1053B](https://octopart.com/part/vlsi/VS1053B-L) - audio processing Integrated Circuit (IC)
    - [ABM3B-12.288MHZ-10-1-U-T](https://www.mouser.com/ProductDetail/ABRACON/ABM3B-12.288MHZ-10-1-U-T?qs=76dmnhCH%2FMMXut1jnJxJwg%3D%3D&utm_source=octopart&utm_medium=aggregator&utm_campaign=815-ABM3B-12.288-T&utm_content=ABRACON) - 12.288 MHz Crystal Oscillator
    - [MIC94310-GYM5-TR](https://octopart.com/part/microchip/MIC94310-GYM5-TR?utm_source=altium_a365_bom_portal) - 1.8V LDO Regulator
    - [PJ-320A](https://octopart.com/part/xkb-connectivity/PJ-320A?utm_source=altium_a365_bom_portal) - Headphone Jack
- File Storage:
    - [502570-0893](https://www.digikey.com/en/products/detail/molex/5025700893/1866792?msockid=2623ad1ec3e0601e30edbb6bc2936107) - MicroSD_card slot
- Display:
    - [NHD-2.4-240320AF-CSXP](https://www.digikey.com/en/products/detail/newhaven-display-intl/NHD-2.4-240320AF-CSXP/22204988?curr=usd&utm_campaign=buynow&utm_medium=aggregator&utm_source=octopart) - TFT-LCD Color Display
    - [54132-4062](https://octopart.com/part/molex/54132-4062?utm_source=altium_a365_bom_portal) - Display connector
    - [AO3400A](https://octopart.com/part/alpha-omega-semiconductor/AO3400A?utm_source=altium_a365_bom_portal) - Backlight-dimming Mosfet
- Power System:
    - [1054550101](https://www.digikey.com/en/products/detail/molex-llc/1054550101/8575770?curr=usd) - USB-C Connector
    - [BQ24074RGTT](https://octopart.com/part/texas-instruments/BQ24074RGTT?utm_source=altium_a365_bom_portal) - Charging IC
    - 3.7V Li‑Po Battery
    - [AP2112K-3.3TRG1](https://octopart.com/part/diodes-inc/AP2112K-3.3TRG1?utm_source=altium_a365_bom_portal) - 3.3V LDO Regulator
    - [S2B-PH-K-S(LF)(SN)](https://octopart.com/part/jst/S2B-PH-K-S%28LF%29%28SN%29?utm_source=altium_a365_bom_portal) - Battery Connector
- Protocol Bridges
    - [CP2102N-A02-GQFN24R](https://octopart.com/part/silicon-labs/CP2102N-A02-GQFN24R?utm_source=altium_a365_bom_portal) - USB-to-UART Bridge
    - [GL823K-HCY04](https://octopart.com/part/genesys/GL823K-HCY04?utm_source=altium_a365_bom_portal) - USB-to-SDIO Bridge
    - [FSUSB43L10X](https://octopart.com/part/onsemi/FSUSB43L10X?utm_source=altium_a365_bom_portal) - 2x4 MUX
    - [TS3A27518EPWR](https://octopart.com/part/texas-instruments/TS3A27518EPWR?utm_source=altium_a365_bom_portal) - 6x12 MUX
    - [UMH3NTN](https://octopart.com/part/rohm/UMH3NTN?utm_source=altium_a365_bom_portal) - Dual Transistor enabling resetting from IDEs
    - [SP0503BAHTG](https://octopart.com/part/littelfuse/SP0503BAHTG?utm_source=altium_a365_bom_portal) - Avalanche Diodes as a surge protector
- User Input: navigation buttons (Up, Down, Left, Right, select), volume control, playback mode, power, home, back
    - [PTA4543-2015DPB103](https://www.digikey.com/en/products/detail/bourns-inc/PTA4543-2015DPB103/3534245?curr=usd&utm_campaign=buynow&utm_medium=aggregator&utm_source=octopart) - Volume Slide switch/slider
    - [COM-26850](https://octopart.com/part/sparkfun/COM-26850?utm_source=altium_a365_bom_portal) - Navigational D-Pad (with select center)
    - [SKQGABE010](https://octopart.com/part/alps/SKQGABE010?utm_source=altium_a365_bom_portal) - General buttons (Power, playback mode, home, back)

---
## 💿 Programming
We have set up our project for ESP-IDF use with a main component and secondary components, named to fit the functions and different pieces of our device. 

Each component has a CMakeLists.txt file that describes which files are source files for the component and which are header/include files, as well as which other components it depends on (using "REQUIRES" inside of idf_component_register). There is no difference to programming at this step, files can be created for each component just make sure to add them to the CMakeLists.txt file. If a folder/directory is made for header files, make sure to swap out the "." for the folder's name in INCLUDE_DIRS.

Alongside ESP-IDF we are using Docker and its dockerfile to minimize the amount of bloat on team computers. The dockerfile will run using Espressif's docker image, which has a good amount of their Software Development Kit (SDK) already installed or written to install on it. I said the last part because, as we are using their image  for our needs, we are also creating an image, runnning up an instance of it to build our firmware, and then deleting it. Espressif did the first half, but instead of deleting it, it iss publicly availble to be sourced in a dockerfile for projects needing Espressif's development framework(s).

To build our firmware there should only be two steps, thanks to Espressi'f socker image:

(1) Run this command 
```docker run --rm -v $PWD:/project -w /project espressif/idf:latest idf.py build```

(2) Click flash button on IDE or run Espressif's flash.py command (which would need Python since it is a python script)

The build command may be modified, as well as the rest of the dockerfile. To add libraries for the firmware either add an install step in the dockerfile or add the library as a dependecy in idf_component_yaml in main folder.

Make a branch off of main when first starting on a feature so that the base repository is safe and working.

---
## 👥 Team  
- William K. Coleman (Lead)
- Paola Dorado Galicia 
- Diego Cruz
- Rodolfo Garcia
- 
- 
- 
- 
- 
- 
- 
- 
- 
---

