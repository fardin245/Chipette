#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "include/SDL2/SDL.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID device;
} sdl_t;

typedef struct {
    uint16_t opcode;        // Opcode of the instruction
    uint16_t NNN;           // Lowest 12 bits of the instruction
    uint8_t NN;             // Lowest 8 bits of the instruction
    uint8_t N;              // Lowest 4 bits of the instruction
    uint8_t X;              // Lower 4 bits of the high byte of the instruction
    uint8_t Y;              // Upper 4 bits of the low byte of the instruction
} instruction_t;

typedef struct {
    uint32_t window_width;  // Pixel width of the origin chip-8
    uint32_t window_height; // Pixel height of the original chip-8
    uint32_t window_scale;  // Window size scaling
    uint32_t emulation_rate; // number of instructions to read per second
    uint8_t memory[4096];   // Chip-8 ram of 4KB (4096 bytes)
    bool display[64 * 32];  // Display size 64 * 32
    uint8_t V[16];          // Registers
    uint16_t I;             // Index Pointer
    uint16_t PC;            // Program Counter
    uint16_t stack[16];     // Stack
    uint16_t *SP;           // Stack Pointer
    uint8_t delay_timer;    // Delay Timer
    uint8_t sound_timer;    // Sound Timer
    bool keypad[16];        // Keypad for button input
    uint8_t state;          // State = Active, Paused, Quit
    bool debug_state;       // Determines whether debug information is shown
    uint8_t mode;           // Swaps between the original chip-8, superchip, and xo-chip
    bool draw;              // Determines whether the display will refresh or not
} chip8_t;

// Initialize chip-8 configuration to default values
bool initialize_chip8(chip8_t *chip8, const char rom_name[]) {
    chip8->window_width = 64;
    chip8->window_height = 32;
    chip8->window_scale = 12;
    chip8->emulation_rate = 600;
    memset(chip8->memory, 0, sizeof(chip8->memory));
    memset(chip8->display, 0, sizeof(chip8->display));
    memset(chip8->V, 0, sizeof(chip8->V));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->keypad, 0, sizeof(chip8->keypad));
    chip8->I = 0;
    chip8->PC = 0x200;
    chip8->SP = &chip8->stack[0];
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    chip8->state = 1;
    chip8->debug_state = 0;
    chip8->mode = 0;
    const uint8_t font[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80    // F
    };
    memcpy(chip8->memory, font, sizeof(font));
    // Read rom file
    FILE *rom = fopen(rom_name, "rb");
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->memory - 0x200;
    if (rom_size > max_size) {
        printf("File too large to open. Maximum allowable file size: %lu bytes. Current file size: %lu bytes\n", max_size, rom_size);
        return false;
    }
    rewind(rom);
    fread(&chip8->memory[0x200], rom_size, 1, rom);
    fclose(rom);
    return true;
}

// Audio Control
void audio_callback(void *userdata, uint8_t *stream, int len) {
    int16_t *audio_data = (int16_t *)stream;
    static uint32_t sampleindex = 0;
    const uint32_t frequency = 600;
    const uint32_t samplerate = 44100;
    const int volume = 3000;
    for (int i = 0; i < len / 2; i++) {
        audio_data[i] = ((sampleindex++ / ((samplerate / frequency) / 2)) % 2) ? volume : -volume;
    }
}

// Initialize SDL2 dependencies
void initialize_sdl(sdl_t *sdl, chip8_t *chip8) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        printf("SDL Initialization Error: %s\n", SDL_GetError());
    }
    sdl->window = SDL_CreateWindow("Chipette", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, chip8->window_width * chip8->window_scale, chip8->window_height * chip8->window_scale, 0);
    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    sdl->want = (SDL_AudioSpec) {.freq = 44100, .format = AUDIO_S16LSB, .channels = 1, .samples = 512, .callback = audio_callback};
    sdl->device = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);
}

// Update timers at a rate of 60Hz
void update_timers(sdl_t *sdl, chip8_t *chip8) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }
    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl->device, 0);
    }
    else {
        SDL_PauseAudioDevice(sdl->device, 1);
    }
}

// Clear the screen
void clear_screen(sdl_t *sdl) {
    SDL_SetRenderDrawColor(sdl->renderer, 20, 20, 20, 255);
    SDL_RenderClear(sdl->renderer);
}

// Update the screen
void update_screen(sdl_t *sdl, chip8_t *chip8) {
    SDL_Rect pixel = {.x = 0, .y = 0, .w = chip8->window_scale, .h = chip8->window_scale};
    // Draw the pixels on the display
    for (uint32_t i = 0; i < chip8->window_width * 32; i++) {
        pixel.x = (i % chip8->window_width) * pixel.w;
        pixel.y = (i / chip8->window_width) * pixel.h;
        if (chip8->display[i]) {
            SDL_SetRenderDrawColor(sdl->renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(sdl->renderer, &pixel);
        }
        else {
            SDL_SetRenderDrawColor(sdl->renderer, 20, 20, 20, 255);
            SDL_RenderFillRect(sdl->renderer, &pixel);
        }
    }
    SDL_RenderPresent(sdl->renderer);
}

// Quit everything
void quit_all(sdl_t *sdl) {
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_CloseAudioDevice(sdl->device);
    SDL_Quit();
}

// Handle keyboard input
void handle_input(chip8_t *chip8, const char rom_name[]) {
    const uint8_t key_map[16] = {
        SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,     // 0, 1, 2, 3
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,     // 4, 5, 6, 7
        SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,     // 8, 9, A, B
        SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V      // C, D, E, F
    };
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:  // End program when exiting
                chip8->state = 0;
                break;
            case SDL_KEYDOWN:
                for (int i = 0; i < 16; i++) {
                    if (event.key.keysym.scancode == key_map[i]) {
                        chip8->keypad[i] = true;
                        break;
                    }
                    else if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {            // Allow exiting with ESC key
                        chip8->state = 0;
                        break;
                    }
                    else if (event.key.keysym.scancode == SDL_SCANCODE_P) {                 // P to Pause/Unpause
                        if (chip8->state == 1) {
                            chip8->state = 2;
                            printf("PAUSED\n");
                        }
                        else if (chip8->state == 2) {
                            chip8->state = 1;
                            printf("UNPAUSED\n");
                        }
                        break;
                    }
                    else if (event.key.keysym.scancode == SDL_SCANCODE_T) {                 // Restart rom
                        initialize_chip8(chip8, rom_name);
                        break;
                    }
                    else if (event.key.keysym.scancode == SDL_SCANCODE_B) {                 // Enable/Disable debug information
                        if (chip8->debug_state == 0) {
                            chip8->debug_state = 1;
                            chip8->emulation_rate = 1;
                            printf("DEBUG MODE ACTIVATED\n");
                        }
                        else if (chip8->debug_state == 1) {
                            chip8->debug_state = 0;
                            chip8->emulation_rate = 600;
                            printf("DEBUG MODE DEACTIVATED\n");
                        }
                        break;
                    }
                    else if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {               // Swap between Chip-8, Superchip, and XO-Chip modes
                        uint8_t chip_mode = chip8->mode;
                        if (chip_mode == 0) {
                            chip8->mode = 1;
                            printf("CHIP MODE: SUPERCHIP\n");
                        }
                        else if (chip_mode == 1) {
                            chip8->mode = 2;
                            printf("CHIP MODE: XO-CHIP\n");
                        }
                        else if (chip_mode == 2) {
                            chip8->mode = 0;
                            printf("CHIP MODE: CHIP-8\n");
                        }
                        break;
                    }
                }
                break;
            case SDL_KEYUP:
                for (int i = 0; i < 16; i++) {
                    if (event.key.keysym.scancode == key_map[i]) {
                        chip8->keypad[i] = false;
                        break;
                    }
                }
                break;
        }
    }
}

// Emulate one instruction
void emulate_instruction(chip8_t *chip8, instruction_t *instruction) {
    bool carry;
    instruction->opcode = (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
    if (chip8->debug_state) {
        printf("The current instruction is at Address: 0x%04X with opcode: 0x%04X\n", chip8->PC-2, instruction->opcode);
    }
    chip8->PC += 2;
    instruction->NNN = instruction->opcode & 0x0FFF;                                        // Mask upper 4 bits
    instruction->NN = instruction->opcode & 0x00FF;                                         // Mask upper 8 bits
    instruction->N = instruction->opcode & 0x000F;                                          // Mask upper 12 bits
    instruction->X = (instruction->opcode >> 8) & 0x0F;                                     // Shift 8 bits to the right and then mask
    instruction->Y = (instruction->opcode >> 4) & 0x0F;                                     // Shift 4 bits to the right and then mask
    switch (instruction->opcode & 0xF000) {
        case 0x0000:
            if (instruction->NN == 0xE0) {                                                  // 0x00E0
                for (int i = 0; i < chip8->window_width * chip8->window_height; i++) {
                    chip8->display[i] = false;
                }
                chip8->draw = true;
            }
            else if (instruction->NN == 0xEE) {                                             // 0x00EE
                chip8->PC = *--chip8->SP;
            }
            break;
        case 0x1000:                                                                        // 0x1NNN
            chip8->PC = instruction->NNN;
            break;
        case 0x2000:                                                                        // 0x2NNN
            *chip8->SP++ = chip8->PC;
            chip8->PC = instruction->NNN;
            break;
        case 0x3000:                                                                        // 0x3XNN
            if (chip8->V[instruction->X] == instruction->NN) {
                chip8->PC += 2;
            }
            break;
        case 0x4000:                                                                        // 0x4XNN
            if (chip8->V[instruction->X] != instruction->NN) {
                chip8->PC += 2;
            }
            break;
        case 0x5000:                                                                        // 0x5XY0
            if (chip8->V[instruction->X] == chip8->V[instruction->Y]) {
                chip8->PC += 2;
            }
            break;
        case 0x6000:                                                                        // 0x6XNN
            chip8->V[instruction->X] = instruction->NN;
            break;
        case 0x7000:                                                                        // 0x7XNN
            chip8->V[instruction->X] += instruction->NN;
            break;
        case 0x8000:
            switch (instruction->N) {
                case 0:                                                                     // 0x8XY0
                    chip8->V[instruction->X] = chip8->V[instruction->Y];
                    break;
                case 1:                                                                     // 0x8XY1
                    chip8->V[instruction->X] |= chip8->V[instruction->Y];
                    chip8->V[0xF] = 0;
                    break;
                case 2:                                                                     // 0x8XY2
                    chip8->V[instruction->X] &= chip8->V[instruction->Y];
                    chip8->V[0xF] = 0;
                    break;
                case 3:                                                                     // 0x8XY3
                    chip8->V[instruction->X] ^= chip8->V[instruction->Y];
                    chip8->V[0xF] = 0;
                    break;
                case 4:                                                                     // 0x8XY4
                    carry = ((uint16_t)(chip8->V[instruction->X] + chip8->V[instruction->Y]) > 255);
                    chip8->V[instruction->X] += chip8->V[instruction->Y];
                    chip8->V[0xF] = carry;
                    break;
                case 5:                                                                     // 0x8XY5
                    carry = (chip8->V[instruction->Y] <= chip8->V[instruction->X]);
                    chip8->V[instruction->X] -= chip8->V[instruction->Y];
                    chip8->V[0xF] = carry;
                    break;
                case 6:                                                                     // 0x8XY6
                    carry = chip8->V[instruction->Y] & 1;
                    chip8->V[instruction->X] = chip8->V[instruction->Y] >> 1;
                    chip8->V[0xF] = carry;
                    break;
                case 7:                                                                     // 0x8XY7
                    carry = (chip8->V[instruction->X] <= chip8->V[instruction->Y]);
                    chip8->V[instruction->X] = chip8->V[instruction->Y] - chip8->V[instruction->X];
                    chip8->V[0xF] = carry;
                    break;
                case 0xE:                                                                   // 0x8XYE
                    carry = (chip8->V[instruction->Y] & 0x80) >> 7;
                    chip8->V[instruction->X] = chip8->V[instruction->Y] << 1;
                    chip8->V[0xF] = carry;
                    break;
                default:
                    break;
            }
            break;
        case 0x9000:                                                                        // 0x9XY0
            if (chip8->V[instruction->X] != chip8->V[instruction->Y]) {
                chip8->PC += 2;
            }
            break;
        case 0xA000:                                                                        // 0xANNN
            chip8->I = instruction->NNN;
            break;
        case 0xB000:                                                                        // 0xBNNN
            chip8->PC = chip8->V[0] + instruction->NNN;
            break;
        case 0xC000:                                                                        // 0xCXNN
            chip8->V[instruction->X] = (rand() % 256) & instruction->NN;
            break;
        case 0xD000: {                                                                      // 0xDXYN
            uint8_t x_coordinate = chip8->V[instruction->X] % chip8->window_width;
            uint8_t y_coordinate = chip8->V[instruction->Y] % chip8->window_height;
            const uint8_t x_original = x_coordinate;
            chip8->V[0xF] = 0;
            for (uint8_t i = 0; i < instruction->N; i++) {
                const uint8_t sprite_data = chip8->memory[chip8->I + i];
                x_coordinate = x_original;
                for (int8_t j = 7; j>= 0; j--) {
                    bool *pixel_index = &chip8->display[y_coordinate * chip8->window_width + x_coordinate];
                    const bool sprite_bit = (sprite_data & (1 << j));
                    if (sprite_bit && *pixel_index) {
                        chip8->V[0xF] = 1;
                    }
                    *pixel_index ^= sprite_bit;
                    if (++x_coordinate >= chip8->window_width) {
                        break;
                    }
                }
                if (++y_coordinate >= chip8->window_height) {
                    break;
                }
            }
            chip8->draw = true;
            break;
        }
        case 0xE000:
            if (instruction->NN == 0x9E) {                                                  // 0xEX9E
                if (chip8->keypad[chip8->V[instruction->X]]) {
                    chip8->PC += 2;
                }
            }
            else if (instruction->NN == 0xA1) {                                             // 0xEXA1
                if (!chip8->keypad[chip8->V[instruction->X]]) {
                    chip8->PC += 2;
                }
            }
            break;
        case 0xF000:
            switch (instruction->NN) {
                case 0x0A: {                                                                // 0xFX0A
                    static bool any_key_pressed = false;
                    static uint8_t key = 0xFF;
                    for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypad; i++) {
                        if (chip8->keypad[i]) {
                            key = i;
                            any_key_pressed = true;
                            break;
                        }
                    }
                    if (!any_key_pressed) {
                        chip8->PC -= 2;
                    }
                    else {
                        if (chip8->keypad[key]) {
                            chip8->PC -= 2;
                        }
                        else {
                            chip8->V[instruction->X] = key;
                            key = 0xFF;
                            any_key_pressed = false;
                        }
                    }
                    break;
                }
                case 0x1E:                                                                  // 0xFX1E
                    chip8->I += chip8->V[instruction->X];
                    break;
                case 0x07:                                                                  // 0xFX07
                    chip8->V[instruction->X] = chip8->delay_timer;
                    break;
                case 0x15:                                                                  // 0xFX15
                    chip8->delay_timer = chip8->V[instruction->X];
                    break;
                case 0x18:                                                                  // 0xFX18
                    chip8->sound_timer = chip8->V[instruction->X];
                    break;
                case 0x29:                                                                  // 0xFX29
                    chip8->I = chip8->V[instruction->X] * 5;
                    break;
                case 0x33:                                                                  // 0xFX33
                    uint8_t bcd = chip8->V[instruction->X];
                    chip8->memory[chip8->I + 2] = bcd %10;
                    bcd /= 10;
                    chip8->memory[chip8->I + 1] = bcd %10;
                    bcd /= 10;
                    chip8->memory[chip8->I] = bcd;
                    break;
                case 0x55:                                                                  // 0xFX55
                    for (uint8_t i = 0; i <= instruction->X; i++) {
                        chip8->memory[chip8->I++] = chip8->V[i];
                    }
                    break;
                case 0x65:                                                                  // 0xFX65
                    for (uint8_t i = 0; i <= instruction->X; i++) {
                        chip8->V[i] = chip8->memory[chip8->I++];
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            if (chip8->debug_state == 1) {
                printf("Unimplemented/Invalid opcode: 0x%04X", instruction->opcode);
            }
            break;
    }
}

// Main
int main(int argc, char **argv) {
    const char *rom_name = argv[1];                                                         // Take input for rom name
    clock_t start_time, end_time;
    chip8_t chip8;
    sdl_t sdl;
    initialize_chip8(&chip8, rom_name);
    initialize_sdl(&sdl, &chip8);
    clear_screen(&sdl);
    srand(time(NULL));
    instruction_t instruction;
    while (chip8.state != 0) {                                                              // Loop through the instructions until exiting the program
        handle_input(&chip8, rom_name);
        if (chip8.state == 2) {
            update_screen(&sdl, &chip8);                                                    // Update the screen to show Paused/Unpaused state
            continue;
        }
        start_time = clock();
        for (uint32_t i = 0; i < chip8.emulation_rate; i++) {                               // Emulate upto 600 instructions in a row or until the display needs a refresh
            emulate_instruction(&chip8, &instruction);
            if (instruction.opcode >> 12 == (0xD | 0x0)) {
                break;
            }
        }
        update_screen(&sdl, &chip8);                                                        // Update the screen when 0xDXYN or 0x00E0 instructions are encountered
        chip8.draw = false;
        end_time = clock();
        if (chip8.debug_state == 0) {
            SDL_Delay(16.67f - (end_time - start_time));                                    // Artificial delay to allow for reasonable emulation speed
        }
        else {
            SDL_Delay(1000);
        }
        update_timers(&sdl, &chip8);
    }
    quit_all(&sdl);
    exit(EXIT_FAILURE);                                                                     // Goodbye program
}
