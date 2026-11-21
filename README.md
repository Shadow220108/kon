<h1 align="center">
<br>
<img width="600" alt="kon" src="images/case_front.png" />
<br>
kon
<br>
</h1>

<h3 align="center">

A little ESP32-S3 e-ink reader for books on a microSD card 

</h3>

<div align="center">
  <img width="400" alt="back" src="images/case_back_logo.png" />
  <img width="400" alt="pcb" src="images/render_front.png" />
</div>

kon is a small e-reader I designed from scratch. It has a 4.26 inch 800 x 480 e-ink screen, reads .txt books off a microSD card, and you flip pages with 4 buttons on the side. It runs off a LiPo battery that charges over USB-C, and since e-ink only uses power when the page changes, the battery lasts ages. The PCB is made in KiCad and the case in Fusion 360.



## Features

- ESP32-S3-WROOM-1-N16R8, 16MB flash, 8MB PSRAM, WiFi and Bluetooth, native USB
- 4.26 inch GDEY0426T82 e-paper (800 x 480, SSD1677) on a 24 pin FPC
- On board high voltage boost circuit for the e-paper
- microSD in 4 bit mode, with its own power switch so it's fully off in sleep
- PREV, NEXT, MENU and BACK buttons that can wake it from deep sleep
- Single cell LiPo with an MCP73831 charger (500mA), red / green charge LEDs
- Power path that runs from USB when plugged in and from the battery when not
- Slide power switch and a TLV75533 low dropout 3.3V regulator
- Battery voltage and USB detect so the firmware can show the battery percent
- USB-C with ESD protection and a 500mA fuse
- 3D printed case with button caps and the kon logo on the back

## Schematic

| Full schematic |
| --- |
| <img width="900" alt="full schematic" src="images/full_schematic.png" /> |

| USB-C | Charger and power path |
| --- | --- |
| <img width="400" alt="usb c" src="images/usb_c.png" /> | <img width="500" alt="charger" src="images/charger_power_path.png" /> |

| Power switch and 3.3V | Battery and sensing |
| --- | --- |
| <img width="450" alt="ldo" src="images/power_switch_ldo.png" /> | <img width="400" alt="battery" src="images/battery_sensing.png" /> |

| ESP32-S3 | Reset, boot and buttons |
| --- | --- |
| <img width="450" alt="esp32" src="images/esp32_s3.png" /> | <img width="450" alt="buttons" src="images/reset_boot_buttons.png" /> |

| E-paper interface | microSD |
| --- | --- |
| <img width="450" alt="epd" src="images/epd_interface.png" /> | <img width="400" alt="sd" src="images/microsd.png" /> |


## PCB

The board is 84 x 128 mm with 2 layers, and 4 M2.5 mounting holes. 

| Placement | Fully routed |
| --- | --- |
| <img width="350" alt="placement" src="images/first_placement.png" /> | <img width="350" alt="routed" src="images/full_routed.png" /> |

| E-paper routing | microSD routing | Power routing |
| --- | --- | --- |
| <img width="300" alt="epd routing" src="images/epd_routed.png" /> | <img width="300" alt="sd routing" src="images/microsd_routed.png" /> | <img width="300" alt="power routing" src="images/power_routed.png" /> |

| Top pour | Bottom pour |
| --- | --- |
| <img width="350" alt="top" src="images/pour_top.png" /> | <img width="350" alt="bottom" src="images/pour_bottom.png" /> |

Both layers have a ground pour, with a keep-out under the ESP32 antenna on both. Power traces are 0.5 and 0.6 mm, signals 0.25 mm.

| Render back | Render front |
| --- | --- |
| <img width="450" alt="render back" src="images/render_back.png" /> | <img width="400" alt="render front" src="images/render_front.png" /> |

## Case

| Front | Side | Back |
| --- | --- | --- |
| <img width="300" alt="front" src="images/case_front.png" /> | <img width="200" alt="side" src="images/case_edge_ports.png" /> | <img width="300" alt="back" src="images/case_back.png" /> |

A back tray that the PCB and battery sit in, and a front frame with a window for the screen and 4 button caps. There are cutouts on the side for the USB-C and the power switch. 

## Firmware

The firmware is in `firmware/kon_reader`, written in Arduino with the ESP32 core and GxEPD2.

- Put .txt files in the root of the microSD card
- MENU opens the library, PREV / NEXT move, BACK opens a book
- In a book, PREV / NEXT turn the page and BACK goes back to the library
- It remembers the page you were on for every book
- Battery percent at the bottom, with a + when charging
- After 3 minutes it cuts power to the SD card and goes into deep sleep, any button wakes it up

The firmware was written with the help of AI. I went through it and double checked every pin against the schematic, but it hasn't been tested on the real board yet, so some things might need tweaking.

To flash it, install the "esp32" board package by Espressif and the GxEPD2 library, pick "ESP32S3 Dev Module" with 16MB flash, OPI PSRAM and USB CDC On Boot enabled, hold BOOT, tap RESET and upload over USB-C.


## Why was it made?

I read a lot on my phone and my eyes always hurt after, so I wanted my own e-reader. Making one myself with my own board and case was way more fun than buying one :D

## BOM

| Designator | Value | Part | Qty | Unit Price ($) | Total ($) |
| --- | --- | --- | --- | --- | --- |
| C1 | 4.7nF 0603 | CL10B472KB8NNNC | 1 | 0.0021 | 0.0021 |
| C10 | 22uF 0805 | CL21A226MAQNNNE | 1 | 0.0213 | 0.0213 |
| C7-C9, C11, C13-C16, C28, C30 | 100nF 0603 | CL10B104KB8NNNC | 10 | 0.0036 | 0.0360 |
| C12, C25-C27 | 1uF 0603 | CL10A105KB8NNNC | 4 | 0.0046 | 0.0184 |
| C2, C3, C17 | 4.7uF 0603 | CL10A475KO8NNNC | 3 | 0.0081 | 0.0243 |
| C18-C24 | 1uF 50V 0805 | CL21B105KBFNNNE | 7 | 0.0118 | 0.0826 |
| C4-C6, C29 | 10uF 0805 | CL21A106KAYNNNE | 4 | 0.0162 | 0.0648 |
| D1 | Red LED 0603 | KT-0603R | 1 | 0.0121 | 0.0121 |
| D2 | Green LED 0603 | KT-0603G | 1 | 0.0124 | 0.0124 |
| D3 | Schottky | B5819W | 1 | 0.0150 | 0.0150 |
| D4, D5, D6 | Schottky | MBR0530T1G | 3 | 0.0310 | 0.0930 |
| F1 | 500mA polyfuse 1206 | SMD1206P050TF | 1 | 0.0298 | 0.0298 |
| J1 | USB-C | TYPE-C-31-M-12 | 1 | 0.0975 | 0.0975 |
| J2 | LiPo connector | S2B-PH-SM4-TB | 1 | 0.1172 | 0.1172 |
| J3 | 24 pin FPC | FH12-24S-0.5SH(55) | 1 | 0.3876 | 0.3876 |
| J4 | microSD slot | DM3AT-SF-PEJM5 | 1 | 0.7732 | 0.7732 |
| L1 | 10uH inductor | SRN4018-100M | 1 | 0.1857 | 0.1857 |
| Q1, Q3 | P-MOSFET | AO3401A | 2 | 0.0446 | 0.0892 |
| Q2 | N-MOSFET | SI1308EDL-T1-GE3 | 1 | 0.1169 | 0.1169 |
| R1, R2 | 5.1k 0603 | RC0603FR-075K1L | 2 | 0.0013 | 0.0026 |
| R7, R10, R11, R14-R17, R24 | 100k 0603 | RC0603FR-07100KL | 8 | 0.0013 | 0.0104 |
| R12, R13, R19-R23 | 10k 0603 | RC0603FR-0710KL | 7 | 0.0013 | 0.0091 |
| R18 | 2.2R 0603 | RC0603FR-072R2L | 1 | 0.0015 | 0.0015 |
| R3, R8, R9 | 1M 0603 | RC0603FR-071ML | 3 | 0.0013 | 0.0039 |
| R4 | 2k 0603 | RC0603FR-072KL | 1 | 0.0013 | 0.0013 |
| R5, R6 | 1k 0603 | RC0603FR-071KL | 2 | 0.0013 | 0.0026 |
| SW1 | Slide switch | PCM12SMTR | 1 | 0.7199 | 0.7199 |
| SW2, SW3 | Tactile switch | PTS810SJM250SMTRLFS | 2 | 0.1980 | 0.3960 |
| SW4-SW7 | Tactile switch | PTS645SM43SMTR92 | 4 | 0.2432 | 0.9728 |
| U1 | ESP32-S3 module | ESP32-S3-WROOM-1-N16R8 | 1 | 3.4726 | 3.4726 |
| U2 | USB ESD | USBLC6-2SC6 | 1 | 0.0796 | 0.0796 |
| U3 | LiPo charger | MCP73831T-2ACI/OT | 1 | 0.4665 | 0.4665 |
| U4 | 3.3V LDO | TLV75533PDBVR | 1 | 0.0680 | 0.0680 |
| Display | 4.26in e-paper 800x480 | GDEY0426T82 | 1 | 32.0000 | 32.0000 |
| Battery | 3.7V 2000mAh LiPo, JST-PH | Adafruit 2011 | 1 | 12.5000 | 12.5000 |
| | Components | | | | 52.89 |
| | PCBA (2 boards, JLCPCB) | | 1 | 76.00 | 76.00 |
| | Total | | | | 128.89 |
