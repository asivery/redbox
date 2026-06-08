# RedBOX

The open-source programmer for Renesas GreenPAK chips with OTP memory.

## Project layout

- `hardware` - contains the schematics and sample PCB design for the programmer
- `software/host` - contains the program which you should run on your PC to communicate with the RedBOX board
- `software/usb` - contains the program which should be written to the RP2040 board

## Compatibility list

**TODO**

Right now no library files are published, as I still need to do more testing before I publish any of them.

## Technical

### Connection with the programmer

The RedBOX programming software communicates with the RedBOX board using a vendor USB interface - make sure you have the appropriate device drivers installed (and access rules set up). 

### The GreenPAK protocol

In the GreenPAK ICs, the OTP is programmed with the help of PCTL and PROG pins. In order to force the GreenPAK into a programming mode, one needs to apply 7.5V to the PROG pin. Even if programmed to disable the built-in I2C interface, after applying that voltage, the GreenPAK should report back, if queried over I2C.

I have observed that before writing the actual program, the Renesas dev board sends a particular sequence, which I decided to call the init sequence, to the chip. I do not know if that is something that needs to be sent to the chip before programming or not, but since the memory is one-time-programmable I decided not to verify that and instead faithfully recreated that initialization sequence's timings in the RP2040 code. Should the need arise, it is possible to define more initialization sequences - the one I mentioned has the ID of `0`. Every GreenPAK description file should define what init sequence the chip uses.

#### Programming the GreenPAK

To program the GreenPAK, one needs to first send the initialization sequence, then bring up power and the 7.5V on `PROG`.
Afterwards, the `PCTL` pin should be set high, and finally the program can be sent over I2C, with the appropriate padding at the front and end of the memory image.

#### Reading data back

To read the program back, one can simply send the first address of the region to be read back, and start reading data over I2C. If the OTP-written program disabled I2C, the data can still be read after sending 7.5V to the `PROG` pin.
**The chip's I2C ID might change in comparison to what it had been before writing OTP!**

It is possible to disable the ability to read the program bytes from the GreenPAK. If this feature is enabled in the image written to the OTP, the program bytes will instead get read back as `0xFF`.

## Credits

- [Pico_WS2812](https://github.com/ForsakenNGS/Pico_WS2812) - Library for controlling the LED on the RP2040 board
- [RP2040-Zero-Kicad](https://github.com/CountParadox/RP2040-Zero-Kicad), [MT3608-module-kicad](https://github.com/barafael/MT3608-module-kicad) - KiCad symbols and footprints for the Waveshare RP2040 Zero board and MT3608 boost converter

