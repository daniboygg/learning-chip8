#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

#define MEM_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_SCALE 15
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

#define DEBUG_PANEL_WIDTH 640
#define DEBUG_PANEL_HEIGHT 400
#define MARGIN 10
#define FONT_SIZE 20


// INIT EMULATOR
typedef struct {
    uint8_t timer_delay; // decrement if > 0 60 times per second
    uint8_t timer_sound; // decrement if > 0 60 times per second
    uint16_t pc;
    uint16_t register_i;
    uint8_t registers_v[16]; // V0 - VF
    uint16_t stack[16];
    uint8_t memory[MEM_SIZE];
} Chip8;

void load_font_in_memory(uint8_t *memory) {
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80, // F
    };
    const size_t n = sizeof(font) / sizeof(font[0]);
    uint16_t i = 0x50;
    for (size_t j = 0; j < n; j++) {
        memory[i++] = font[j];
    }
}

void load_rom_in_memory(uint8_t *memory) {
    // FILE *rom = fopen("data/1-chip8-logo.ch8", "rb");
    FILE *rom = fopen("data/2-ibm-logo.ch8", "rb");
    if (rom == NULL) {
        fprintf(stderr, "Could not open ROM file\n");
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_END);
    size_t size = ftell(rom);
    if (size > MEM_SIZE - 0x200) {
        fprintf(stderr, "ROM is too large to fit in memory (%ld bytes, max %d)\n", size, MEM_SIZE - 0x200);
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_SET);
    fread(memory + 0x200, sizeof(memory[0]), size, rom);
    fprintf(stdout, "ROM loaded, %lu bytes loaded\n", size);
    fclose(rom);
}

// END EMULATOR

// INIT DEBUG UTILITIES
#define DEBUG_INSTRUCTION_SIZE 8

typedef struct {
    uint16_t instructions[DEBUG_INSTRUCTION_SIZE];
    Chip8 *chip;
} Debug;

void debug_instruction_add(Debug *debug, uint16_t instruction) {
    // Show current instruction and the 9 most recent ones
    for (int i = DEBUG_INSTRUCTION_SIZE - 2; i >= 0; i--) {
        debug->instructions[i + 1] = debug->instructions[i];
    }
    debug->instructions[0] = instruction;
}

// END DEBUG UTILITIES

// INIT RAYLIB UTILITIES
void init_display() {
    InitWindow(
        DISPLAY_WIDTH * DISPLAY_SCALE + DEBUG_PANEL_WIDTH,
        DISPLAY_HEIGHT * DISPLAY_SCALE + DEBUG_PANEL_HEIGHT,
        "CHIP-8"
    );
    SetTargetFPS(60);
}

bool quit_pressed() {
    return WindowShouldClose() || IsKeyPressed(KEY_CAPS_LOCK);;
}

void draw_display(uint8_t *buffer, Debug *debug) {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Emulator screen
    const Color display_background = {20, 20, 20, 255};
    DrawRectangle(
        0,
        0,
        DISPLAY_WIDTH * DISPLAY_SCALE,
        DISPLAY_HEIGHT * DISPLAY_SCALE,
        display_background
    );
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (buffer[y * DISPLAY_WIDTH + x]) {
                DrawRectangle(x * DISPLAY_SCALE, y * DISPLAY_SCALE, DISPLAY_SCALE, DISPLAY_SCALE, LIGHTGRAY);
            }
        }
    }

    // instructions
    int32_t x_start = DISPLAY_WIDTH * DISPLAY_SCALE + MARGIN;
    int32_t y_start = MARGIN;
    DrawText(
        "INSTRUCTIONS\n",
        x_start,
        y_start,
        FONT_SIZE,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    for (int i = 0; i < DEBUG_INSTRUCTION_SIZE; i++) {
        if (i == 0) {
            DrawText(TextFormat("-> 0x%04X\n", debug->instructions[i]), x_start, y_start + FONT_SIZE * i, FONT_SIZE,
                     LIGHTGRAY);
        } else {
            DrawText(TextFormat("   0x%04X\n", debug->instructions[i]), x_start, y_start + FONT_SIZE * i, FONT_SIZE,
                     LIGHTGRAY);
        }
    }
    y_start += FONT_SIZE * DEBUG_INSTRUCTION_SIZE + MARGIN;

    DrawText(
        TextFormat("PC: 0x%08X\n", debug->chip->pc),
        x_start,
        y_start,
        FONT_SIZE,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;
    DrawText(
        TextFormat("R I: 0x%08X\n", debug->chip->register_i),
        x_start,
        y_start,
        FONT_SIZE,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    // registers
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 2; ++j) {
            DrawText(
                TextFormat("V%X %02X\n", j * 8 + i, debug->chip->registers_v[j * 8 + i]),
                x_start + 100 * j,
                y_start + FONT_SIZE * i,
                FONT_SIZE,
                LIGHTGRAY
            );
        }
    }

    // memory layout
    x_start = MARGIN;
    y_start = DISPLAY_HEIGHT * DISPLAY_SCALE + MARGIN;

    DrawText(
        "MEMORY 0x",
        x_start,
        y_start,
        FONT_SIZE,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    size_t rows = 15;
    size_t cols = 16;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            DrawText(
                TextFormat("%02X\n", debug->chip->memory[0x200 + (i * cols + j)]),
                x_start + 40 * j,
                y_start + FONT_SIZE * i,
                FONT_SIZE,
                LIGHTGRAY
            );
        }
    }

    EndDrawing();
}

// END RAYLIB UTILITIES

int main(void) {
    Chip8 chip = {0};
    chip.pc = 0x200;

    uint8_t display_buffer[DISPLAY_BUFFER_SIZE] = {0};
    init_display();

    load_font_in_memory(chip.memory);
    load_rom_in_memory(chip.memory);

    Debug debug = {0};
    debug.chip = &chip;
    uint16_t instruction = 0;

    while (!quit_pressed()) {
        if (IsKeyPressed(KEY_SPACE)) {
            // fetch instruction from memory at pc address
            instruction = (chip.memory[chip.pc] << 8) | chip.memory[chip.pc + 1];
            chip.pc += 2;

            debug_instruction_add(&debug, instruction);

            // decode
            uint8_t nibble_0 = (instruction & 0xF000) >> 12;
            switch (nibble_0) {
                case 0x0: // clear screen
                    for (int i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
                        display_buffer[i] = 0;
                    }
                    break;
                case 0x1: // 1NNN jump
                    chip.pc = instruction & 0x0FFF;
                    break;
                case 0x6: // 6XNN set register VX to NN
                    chip.registers_v[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
                    break;
                case 0x7: {
                    // 7XNN Add the value NN to VX
                    uint8_t x = chip.registers_v[(instruction & 0x0F00) >> 8];
                    chip.registers_v[(instruction & 0x0F00) >> 8] = x + (instruction & 0x00FF);
                    break;
                }
                case 0xA: // ANNN set index register I to NNN
                    chip.register_i = instruction & 0x0FFF;
                    break;
                case 0xD: {
                    // DXYN display
                    uint8_t x = chip.registers_v[(instruction & 0x0F00) >> 8] % DISPLAY_WIDTH;
                    uint8_t y = chip.registers_v[(instruction & 0x00F0) >> 4] % DISPLAY_HEIGHT;

                    chip.registers_v[0xF] = 0;
                    uint8_t n = instruction & 0x000F;
                    for (int i = 0; i < n; i++) {
                        uint8_t sprite[8] = {0};
                        uint8_t mask = 1;
                        for (int k = 7; k >= 0; k--) {
                            sprite[k] = chip.memory[chip.register_i + i] & mask;
                            mask = mask << 1;
                        }
                        for (int j = 0; j < 8; j++) {
                            uint8_t screen_pixel = display_buffer[(y + i) * DISPLAY_WIDTH + x + j];
                            uint8_t sprite_pixel = sprite[j];
                            if (sprite_pixel && screen_pixel) {
                                display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 0;
                                chip.registers_v[0xF] = 1;
                            }
                            if (sprite_pixel && !screen_pixel) {
                                display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 1;
                                chip.registers_v[0xF] = 1;
                            }
                        }
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Wrong instruction 0x%04X!\n", instruction);
                    exit(EXIT_FAILURE);
            }
        }

        draw_display(display_buffer, &debug);

        // limit to 700 CHIP-8 instructions per second
    }

    return 0;
}
