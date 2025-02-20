# Chipette
Chip-8 Interpreter written in C. 

## State of Development
Currently playable. All standard instructions of the original chip-8 work. Passes all flags and quirks for the chip-8 according to the test suite at [Timendus' Chip-8 Test Suite](https://github.com/Timendus/chip8-test-suite?tab=readme-ov-file#chip-8-test-suite). 

![Screenshot 2025-02-20 170242](https://github.com/user-attachments/assets/83b3c014-e1be-4d1f-a739-389a61b35c1d)

The interpreter displays current state of emulation (Active/Paused), debug state (enabled/disabled), and different platforms (Chip-8/SuperChip/XO-Chip). Of the platforms, only the original Chip-8 is implemented currently, so changing to SuperChip/XO-Chip won't do anything. Debug mode slows down emulation to 1 instruction per second and displays debug information on the terminal. 

## To-do:
- Add SuperChip and XO-Chip support
- Improve debug mode
- Add emulation cycles control
- Eliminate flickering

## Controls
The keypad:
|     |     |     |     |
| --- | --- | --- | --- |
| 1   | 2   | 3   | C   |
| 4   | 5   | 6   | D   |
| 7   | 8   | 9   | E   |
| A   | 0   | B   | F   |

Mapped to:
|     |     |     |     |
| --- | --- | --- | --- |
| 1   | 2   | 3   | 4   |
| Q   | W   | E   | R   |
| A   | S   | D   | F   |
| Z   | X   | C   | V   |

Extra Controls:
- ESC  : Quit
- P    : Pause/Unpause
- T    : Reset ROM
- B    : Debug Enable/Disable
- TAB  : Switch Platforms

## Dependencies
- gcc
- sdl2
- roms

## Building
`./project/location make`

## Running
`./chip8 rom`


