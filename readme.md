# ToneLighter

LED stripe controlled by music

## Features

- LED stripe with WS2812 RGB LEDs
- Microphone to catch the music
- Arduino Pro mini which evaluates the audio level and controls the LED stripe
- Second Arduino Pro mini which has a 433 MHz receiver connected, to receive remote control commands
- Serial communication from the one to the other Arduino
- Powered by 3.7V LiIon accus

## Bug List

### Bugs in v2023-05-22 Layout

- R9 value is not printed on PCB (1M)
- R6 value is not printed on PCB (47k)
- C4 value is not printed on PCB and schematic (100n? or bigger?)
- Arduino debug connector is swapped (maybe there are different variants out there???)
