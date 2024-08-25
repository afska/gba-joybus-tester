# gba-joybus-tester

This is a modified version of [NINJA](https://github.com/shonumi/ninja) that lets the Wii homebrew send data to the GBA and print the received values on screen. It was created to test the **LinkCube** library from [gba-link-connection](https://github.com/afska/gba-link-connection). My changes are licensed as GPL-2.0:
- Initialization now uses cmd `0xff` instead of `0x00`.
- Communication is binded with channel `0` (first GC port) instead of `1`.
- The **+** button now increments a number, sends it (cmd `0x15`) and receives (cmd `0x14`) a 32-bit value from the GBA to print it on the screen.
- The **-** button does the same but incrementing by `1024`.
- The file dump feature was removed.

**devkitPPC** and some dependencies are required:

```bash
sudo dkp-pacman -Syu
sudo dkp-pacman -S libogc
sudo dkp-pacman -S libfat-ogc
```

Original project's **README** below:

# NINJA: Receive Data From GBA via JoyBus

NINJA is a generic tool that receives data via JoyBus from a Game Boy Advance. It also makes you a ninja. Use this in conjunction with GBA homebrew to export large amounts of data. Useful for logging data or reverse-engineering unique cartridges (such as the Play-Yan or Campho Advance) and dumping data from ROM. Built for the Wii using DevKitPro.

## Usage
NINJA operates in 32-bit or 16-bit modes. Press the "+" button on the Wii Remote to switch. The default is 32-bit. Whichever mode is necessary depends on the homebrew program running on the GBA. Transfers are displayed on screen as they occur. Data is saved as each transfer completes. Each transfer is appended to the output file "joy_dump.bin". Press the "HOME" button on the Wii Remote to exit NINJA.

## Protocol
GBA homebrew can communicate with NINJA via JoyBus using a simple packet-based protocol. The first value sent is the total length of data being sent in the packet (in terms of 16-bit or 32-bit units, not bytes), followed by the actual data. A packet may not have a length greater than 512 16-bit or 32-bit units, so GBA homebrew may need to break up data greater than 1KB into several packets.

## License
This is Free Open Source Software available under the GPLv2 or later. See license.txt for full details.
