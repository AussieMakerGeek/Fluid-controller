# Fluid-controller
This is a fork of the original Fluid controller but it only retains handfulls of code and implements simple features for controlling my Gravoraph LS800 Co2 Laser that I converted using an MKS-Tinybee 

# Main functions
My implementation uses STM32 Bluepill over direct serial with the MKS-Tinybee using the new secondary serial function in FluidNC. I use the wifi function so bluetooth is not ideal for me, plus I can build this into my machine, no batteries etc.

# Display shows 
Machine coordinates, Work coordinates, Machine status, jog distance and whether X/Y jog or Z jog is active.  Since my machine has a visible laser, I also added a display for it's status.

Main functions of buttons: 
Home, Home Z, Unlock/Reset, Visible laser, Jog distance +, Jog distance -, directional buttons and Z/XY toggle.

No menus...

# Hardware
1. STM32 Bluepill (128k used, about 50% free)
2. ILI9341 based 2.5" display (320x240 res)
3. 11 X TTP223 Touch buttons - The Pad 'A' will need to be shorted to make them active LOW.

# Wiring
The buttons and display were directly wired to the Bluepill, no need for PCB. Mounted in a 3d Printed panel for install to the machine.

# Programming
Programming is done in Arduino IDE. All code exists in one single file. Button definitions set out the pins used.  

Project uses the TFT_eSPI library which requires setting the display parameters in user_setup.h in the library folder:
```
#define STM32
#define ILI9341_DRIVER
#define TFT_SPI_PORT
#define TFT_MOSI PA7
#define TFT_MISO PA6
#define TFT_SCLK PA5
define TFT_CS   PA4
#define TFT_DC   PA2
#define TFT_RST  PA3
#define LOAD_GFXFF
#define SMOOTH_FONT
#define SPI_FREQUENCY  55000000
#define SPI_READ_FREQUENCY  20000000 //Not used
```
It also uses the Multibutton library, but there is an issue in the library for STM32 - you need to eliminate the specific STM32 ifdef's (pinmode call) in PinButton.h or it will not compile.

# Panel
![Front](/Images/2023-08-25_14h56_07.png)
![Back](/Images/2023-08-25_14h55_12.png)
![Front2](/Images/2023-08-25_14h56_35.png)

See STL folder for printable file.

<!--
<img width="440" alt="pendantV2case_lid2" src="https://user-images.githubusercontent.com/20277013/214568520-32bf0ae3-2ae2-4814-8294-004ee3288210.png">
<img width="440" alt="pendantV2lid_case" src="https://user-images.githubusercontent.com/20277013/214570138-59b09fc4-4332-4c2e-8d71-3366ad1cf684.png">
-->

# Assembly
I just added all of the buttons to the panel and used my soldering iron to slightly melt the edges of the plastic to hold them in.  The LCD is held in place with some tape and the BluePill is secured to the back of it with some double sided tape.
I made some vinyl stickers to easily identify the button functions - They didn't turn out perfect but it'll do till my OCD takes over.

# How to operate
The functions are more specific to a Laser than a spindle CNC machine. Hence, the functions have been cut down.

* Directional controls - Move the gantry in the appropriate directions[^1]
* Pressing Z will swap to Z axis movement (only), press again to go back to X/Y mode
* Home Button - Issues $H command, for me, this is only X/Y
* Zhome - $HZ
* Unlock - Similar to the function from the original Fluid-Controller where it sends commands appropriate for the state of the machine to 'unlock' or otherwise 'resume' an Idle state.
* Laser - Issues a GRBL command to turn on a digital output.
* +/- - Increases or decreases the jog distance.

[^1]: Direction will need to be verified for your machine depending on origin etc.

# Other
Serial2 (PB11 / PB10) is used for communication with the machine, Serial(1) (PA9 / PA10) is used for debug output and is not required for operation.

FluidNC will need a line in the config file to enable the second serial terminal:

```
uart1:
  txd_pin: gpio.17
  rxd_pin: gpio.16
  baud: 115200
  mode: 8N1

uart_channel1:
  uart_num: 1
```
See http://wiki.fluidnc.com/en/config/uart_sections for more info
