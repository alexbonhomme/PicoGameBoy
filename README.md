# PicoTracker Game Boy Color

Game Boy and Game Boy Color emulator for the [picoTracker through-hole board](https://github.com/xiphonics/picoTracker) with a Pico 2 (RP2350). This is a hardware port of [stevenpaul007-creator/Pico-GB](https://github.com/stevenpaul007-creator/Pico-GB) (itself based on [YouMakeTech/Pico-GB](https://github.com/YouMakeTech/Pico-GB) and [Peanut-GB](https://github.com/deltabeard/Peanut-GB)). The emulator core is unchanged. Pins, the SD socket, and the PCM5102 audio format are set for this board.

Game Boy Color is enabled (`PEANUT_FULL_GBC_SUPPORT`). Pokemon Gold, Silver, and Crystal are 2 MB ROMs. The `pico2-nopsram` build keeps about 3.5 MB of the Pico 2's 4 MB flash for the ROM, so they fit without PSRAM.

## Pinout

Taken from the picoTracker through-hole schematic.

| Function | GPIO | Board net |
| --- | --- | --- |
| Up | GP11 | SW4 |
| Down | GP9 | SW2 |
| Left | GP8 | SW1 |
| Right | GP10 | SW3 |
| A | GP14 | SW7 |
| B | GP13 | SW6 |
| Select | GP12 | SW8 LT |
| Start | GP16 | SW9 PLAY |
| SD CLK | GP2 | SD_CLK |
| SD CMD / MOSI | GP3 | SD_SI |
| SD D0 / MISO | GP4 | SD_D0 |
| SD D1 | GP5 | pulled up, not used in SPI |
| SD D2 | GP6 | pulled up, not used in SPI |
| SD D3 / CS | GP7 | SD_D3 |
| I2S DIN | GP17 | PCM5102 DIN |
| I2S BCK | GP18 | PCM5102 BCK |
| I2S LRCK | GP19 | PCM5102 LRCK |
| LCD CS | GP20 | LCD_CS |
| LCD DC | GP21 | LCD_CC |
| LCD RST | GP22 | LCD_RST |
| LCD SCK | GP26 | LCD_SCK |
| LCD MOSI | GP27 | LCD_MOSI |
| LCD backlight | GP28 | LCD_LED, active high |

SW5 RT (GP15) is not used. The PCM5102 SCK pin is tied to ground on the module, so the DAC runs from its PLL and needs a 64× sample-rate bit clock. Audio is line level on the line-out jack. This board has no speaker amplifier.

The SD socket is wired for 4-bit SDIO. Firmware talks to it in SPI mode on SPI0 and holds D1 and D2 high.

## SD card

Format the card FAT32. The original Pico-GB readme says to copy `.gb` and `.gbc` ROMs into the root of the card. This port's list opens on `/gb/`, so put the ROMs there. Select (LT) still switches the list to `/nes/`.

Save RAM writes one file, `/SAVES/<rom name>.sav`. The folder is created if it is missing. Example: `/gb/crystal.gbc` saves as `/SAVES/crystal.sav`. Real-time states go in `/rtsav/`.

Loading a game copies the ROM into flash. The Pico 2 flash is rated for at least 100k erase cycles, which is fine for normal play and worth knowing if you relaunch the same game constantly.

## Controls

- LT + PLAY: in-game menu. Save RAM writes `/SAVES/<rom>.sav`. Back to Game List reboots to this list
- LT + B: cycle scale mode (pixel-perfect 160×144, aspect-correct, or stretched)
- Hold LT while powering on: USB bootloader

Scaling matters on this 320×240 SPI panel. The Pico 2 has 512 KB of RAM, which is not enough for two full-size framebuffers plus the emulator, so this build uses one framebuffer. If a full-frame scale is too slow or the image tears, use the pixel-perfect mode.

## Build and flash

PlatformIO environment `pico2-nopsram` is the default. It targets a Pico 2 with no PSRAM.

```sh
pio run -e pico2-nopsram
```

`pio run -e pico2-nopsram` only builds. Flash a running board the same way as play-control:

```sh
pio run -e pico2-nopsram -t upload
```

That uses picotool: it reboots the Pico 2 over USB and writes the UF2. Hold BOOTSEL while plugging in only if the board does not show up as a serial port.

After the first flash, check that the image orientation matches the panel (picoTracker uses ILI9341 MADCTL `0x88`), that the buttons read the right way, that the SD card mounts, and that line-out audio is present.

## Upstream

- [stevenpaul007-creator/Pico-GB](https://github.com/stevenpaul007-creator/Pico-GB)
- [YouMakeTech/Pico-GB](https://github.com/YouMakeTech/Pico-GB)
- [deltabeard/Peanut-GB](https://github.com/deltabeard/Peanut-GB), including the Game Boy Color work from froggestspirit

MIT license. No copyrighted games are included.
