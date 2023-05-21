Motor Board v4 Firmware
=======================

## Instructions

Using a posix system, you require `make`, the `arm-none-eabi` toolchain and `git`.
Before attempting to build anything initialise all the submodules.
```shell
$ git submodule update --init --recursive
```

To build the main binary, run:
```shell
$ make
```
The binary will then be at `src/main.bin`.
This will also build the library libopencm3 the first time you run it.

This can be flashed to an attached motor board that is in bootloader using:
```shell
$ make -C src prog
```
To use the `prog` command you need to install stm32flash. This is a cross-platform utility that may be in your operating system's package manager, otherwise it can be downloaded from [their website](https://sourceforge.net/p/stm32flash/wiki/Home/).

To enter the bootloader the pushbutton on the board can be pressed with the 12V input connected. Whilst 12V power is present the board will remain in bootloader.
Alternatively the bootloader can be entered using the corresponding command.

### Finding the board

With the pyserial library, the serial port can be identified using the `pyserial-ports --verbose` command.

## Controls

The motor board is largely controlled over the serial interface. There is one physical push button on the board that puts the microprocessor into a mode in which new firmware can be installed.

## USB interface

Unlike other SR v4 boards, the STM32 on the motor board does not directly communicate over USB. Instead there is an FTDI USB serial interface chip.

The FTDI Chip has vendor ID `0403` and product ID `6001`. It can be further filtered by the USB `product` field.

Given that the appropriate drivers are available on your computer, it should appear as a standard serial interface. You should open this interface at a baud rate of `115200bps`.

### Serial Commands

Commands can be sent to the board via the serial or USB serial interface. Each command is its own line and is terminated with a new line.

Action | Description | Command | Parameter Description | Return | Return Parameters
--- | --- | --- | --- | --- | ---
Identify | Get the board type and version | *IDN? | - | Student Robotics:MBv4B:\<asset tag>:\<software version> | \<asset tag> <br>\<software version>
Status | Get board status | *STATUS? | - | \<output faults>:\<input voltage> | \<output faults> - a comma separated list of 1/0s indicating if an output driver has reported a fault  e.g. 1,0<br>\<input voltage> - voltage at 12V input in mV
Reset | Reset board to safe startup state<br>- Turn off all outputs<br>- Reset the lights | *RESET | - | ACK | -
Set motor power | Sets the speed of one of the motors | MOT:\<n>:SET:\<value> | \<n> motor number, int, 0-1<br>\<value> motor power, int, -1000 to 1000 | ACK | -
Read motor power | Gets the current speed setting of the motor | MOT:\<n>:GET? | \<n> motor number, int, 0-1 | \<enabled>:\<value> | \<enabled> motor enabled, int, 0-1 <br>\<value> motor power, int, -1000 to 1000"
Disable motor output | Puts the motor into high impedance (equivalent to current coast) | MOT:\<n>:DISABLE | \<n> motor number, int, 0-1 | ACK | -
Read motor current | Read the current power draw of the motor | MOT:\<n>:I? | \<n> motor number, int, 0-1 | \<current> | \<current> - current, int, measured in mA
Enter bootloader | Enter the serial bootloader to load new firmware | *SYS:BOOTLOADER | - | ACK | -

## udev Rule

On most systems this should not be required as serial ports will already below to a non-root group, i.e. plugdev.

If you are connecting the Motor Board to a Linux computer with udev, the following rule can be added in order to access
the Motor Board interface without root privileges:

`SUBSYSTEM=="tty", DRIVERS=="ftdi_sio", ATTRS{interface}=="MCV4B", GROUP="plugdev", MODE="0666"`

It should be noted that `plugdev` can be changed to any Unix group of your preference.

## Designs

You can access the schematics and source code of the hardware in the following places.
-   [Full Schematics](https://studentrobotics.org/docs/resources/kit/motor-schematic.pdf)
-   [Competitor Facing Docs](https://studentrobotics.org/docs/kit/motor_board)
-   [Hardware designs](https://github.com/srobo/motor-v4-hw)
