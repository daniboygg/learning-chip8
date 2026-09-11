#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

#define MEM_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_SCALE 20
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)


void init_display() {
    InitWindow(DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE, "CHIP-8");
    SetTargetFPS(60);
}

bool quit_pressed() {
    return WindowShouldClose() || IsKeyPressed(KEY_CAPS_LOCK);;
}

void draw_display(const uint8_t *buffer) {
    BeginDrawing();
    ClearBackground(BLACK);
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (buffer[y * DISPLAY_WIDTH + x]) {
                DrawRectangle(x * DISPLAY_SCALE, y * DISPLAY_SCALE, DISPLAY_SCALE, DISPLAY_SCALE, WHITE);
            }
        }
    }
    EndDrawing();
}

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
    FILE *rom = fopen("data/1-chip8-logo.ch8", "rb");
    if (rom == NULL) {
        fprintf(stderr, "Could not open ROM file\n");
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_END);
    long size = ftell(rom);
    if (size > MEM_SIZE - 0x200) {
        fprintf(stderr, "ROM is too large to fit in memory (%ld bytes, max %d)\n", size, MEM_SIZE - 0x200);
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_SET);
    fread(memory + 0x200, sizeof(memory[0]), size, rom);
    fclose(rom);
}

int main(void) {
    uint16_t register_i = 0;
    uint8_t registers_v[16] = {0}; // V0 - VF

    uint8_t memory[MEM_SIZE] = {0};
    uint16_t pc = 0x200;
    uint16_t stack[16] = {0};

    uint8_t timer_delay = 0; // decrement if > 0 60 times per second
    uint8_t timer_sound = 0; // decrement if > 0 60 times per second

    uint8_t display_buffer[DISPLAY_BUFFER_SIZE] = {0};
    init_display();

    load_font_in_memory(memory);
    load_rom_in_memory(memory);

    while (!quit_pressed()) {
        // fetch instruction from memory at pc address
        uint16_t instruction = (memory[pc] << 8) | memory[pc + 1];
        pc += 2;

        fprintf(stdout, "Executing instruction: 0x%04X\n", instruction);
        // decode
        uint8_t nibble_0 = (instruction & 0xF000) >> 12;
        switch (nibble_0) {
            case 0x0: // clear screen
                for (int i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
                    display_buffer[i] = 0;
                }
                break;
            case 0x1: // 1NNN jump
                pc = instruction & 0x0FFF;
                break;
            case 0x6: // 6XNN set register VX to NN
                registers_v[(instruction & 0x0F00) >> 8] = instruction & 0x00FF;
                break;
            case 0xA: // ANNN set index register I to NNN
                register_i = instruction & 0x0FFF;
                break;
            case 0xD: // DXYN display
                uint8_t x = registers_v[(instruction & 0x0F00) >> 8] % DISPLAY_WIDTH;
                uint8_t y = registers_v[(instruction & 0x00F0) >> 4] % DISPLAY_HEIGHT;

                registers_v[0xF] = 0;
                uint8_t n = instruction & 0x000F;
                for (int i = 0; i < n; i++) {
                    uint8_t sprite[8] = {0};
                    uint8_t mask = 1;
                    for (int k = 7; k >= 0; k--) {
                        sprite[k] = memory[register_i + i] & mask;
                        mask = mask << 1;
                    }
                    for (int j = 0; j < 8; j++) {
                        uint8_t screen_pixel = display_buffer[(y + i) * DISPLAY_WIDTH + x + j];
                        uint8_t sprite_pixel = sprite[j];
                        if (sprite_pixel && screen_pixel) {
                            display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 0;
                            registers_v[0xF] = 1;
                        }
                        if (sprite_pixel && !screen_pixel) {
                            display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 1;
                            registers_v[0xF] = 1;
                        }
                    }
                }
                break;
            default:
                fprintf(stderr, "Wrong instruction 0x%04X!\n", instruction);
                exit(EXIT_FAILURE);
        }

        draw_display(display_buffer);

        // limit to 700 CHIP-8 instructions per second
    }

    return 0;
}
