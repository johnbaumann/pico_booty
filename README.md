# Pico booty
A barebones implementation of "booty" for PS1 using an rp2040. Booty uses a specially crafted stream of bytes to load and run a payload in place of an external rom(pio cheat cart), using a minimal interface: 8 data pins, chip select, read, and reset.
This project is built on the work of Nicolas "Pixel" Noble who came up with the Booty concept, and danhans42 who's previous work on booty was used a reference for designing my implementation here.

## Pins
| Pico GPIO # | PS1 |
| ----------- | --- |
| 0 | D0 |
| 1 | D1 |
| 2 | D2 |
| 3 | D3 |
| 4 | D4 |
| 5 | D5 |
| 6 | D6 |
| 7 | D7 |
| 8 | CS0 |
| 9 | RD |
| 10 | RESET |

## Creating a payload
You can create a payload using [ps1-packer](https://github.com/grumpycoders/pcsx-redux/tree/main/tools/ps1-packer). A pre-compiled version of this tool is included in releases of [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux).

## Compiling
Compiled using the [Raspberry Pi Pico extension for VS Code](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico), SDK V2.1.1

## To-do
Next steps would be to monitor reset and re-run the payload if the console is restarted.
Additional functionality could be added for uploading payloads over usb.
