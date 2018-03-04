USB floppy controller for Amiga disks
=====================================

This is a USB floppy disk controller, for reading and writing amiga disks. In
conjunction with the accompanying `amigafloppy` program, it allows archiving
amiga disks as ADF disk images, or writing ADF disk images back to disks, to use
with an Amiga computer.

The firmware for this controller is based on the ArduinoFloppyDiskReader
project by Rob Smith (https://github.com/RobSmithDev/ArduinoFloppyDiskReader),
converted to run on standalone AVR hardware (as opposed to an arduino) by John
Tsiombikas. Parts of the `amigafloppy` program are also taken from the original
`AmigaFloppyReader` program by Rob Smith.

The new AVR hardware is designed to be compatible with the original host
software, so you may use the `AmigaFloppyReader` or AmigaFloppyReaderWin`
programs from Rob's project, if you prefer.

Directory structure:

  * `hw` - hardware: kicad files and pdf schematics.
  * `fw` - firmware for the AVR microcontroller.
  * `amigafloppy` - host program for reading/writing ADF images.

Hardware License
----------------
Copyright (C) 2018 John Tsiombikas <nuclear@member.fsf.org>

The hardware of this project is released as free/open hardware under the
Creative Commons Attribution Share-Alike license. See `LICENSE.hw` for details.

Software License
----------------
Copyright (C) 2018 John Tsiombikas <nuclear@member.fsf.org>
Copyright (C) 2017 Rob Smith <rob@robsmithdev.co.uk>

Software components of this project (host program and firmware) are released as
free software, under the terms of the GNU General Public License v3, or later.
See `LICENSE.sw` for details.
