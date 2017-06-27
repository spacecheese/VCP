# VCP
### Multi-platform C serial port library
Originally written to allow cross-platform support for virtualised serial ports from programs written in the C language.
The library is compatible with regular serial ports as well.

The library has been tested on Windows 10, Ubuntu 16.04.1 and Mac OSX, providing coherent results.

### Functionality
The included functions allow:
+ Opening and closing the serial port
+ Writing data to the port
  + Single byte
  + 16-bit word
  + Length-specified buffer
+ Reading data from the port (non-blocking)
  + Buffer up to a specified length
+ Converting between raw hexadecimal and ASCII representation of hexadecimal
