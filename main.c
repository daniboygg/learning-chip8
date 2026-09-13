#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "raylib.h"

#define MEM_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_SCALE 15
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

#define SPRITE_MAX_HEIGHT 15

#define DEBUG_PANEL_WIDTH 640
#define DEBUG_PANEL_HEIGHT 400

#define MARGIN 10
#define FONT_SIZE 20


// INIT EMULATOR
typedef struct {
    bool halt;
    uint8_t timer_delay; // decrement if > 0 60 times per second
    uint8_t timer_sound; // decrement if > 0 60 times per second
    uint16_t pc;
    uint16_t instruction;
    uint16_t register_i;
    uint8_t registers_v[16]; // V0 - VF
    uint16_t stack[16];
    uint8_t memory[MEM_SIZE];
} Chip8;

void chip_load_font(Chip8 *chip) {
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
        chip->memory[i++] = font[j];
    }
}

void chip_reset(Chip8 *chip) {
    memset(chip, 0, sizeof(Chip8));
    chip->pc = 0x200;
    chip_load_font(chip);
}

void chip_load_next_instruction(Chip8 *chip) {
    if (chip->halt) {
        return;
    }
    // fetch instruction from memory at pc address
    chip->instruction = (chip->memory[chip->pc] << 8) | chip->memory[chip->pc + 1];
    chip->pc += 2;
}

size_t chip_load_rom(Chip8 *chip, char *file_path) {
    chip_reset(chip);
    FILE *rom = fopen(file_path, "rb");
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
    fread(chip->memory + 0x200, sizeof(chip->memory[0]), size, rom);
    fclose(rom);
    return size;
}

// END EMULATOR

// INIT DEBUG UTILITIES
#define DEBUG_INSTRUCTION_SIZE 8

typedef struct {
    bool show_registers_decimal;
    size_t rom_size;
    char *rom_loaded_message[512];
    uint16_t instructions[DEBUG_INSTRUCTION_SIZE];
    Chip8 *chip;
} Debugger;

void debugger_instruction_add(Debugger *debug, uint16_t instruction) {
    // Show current instruction and the 9 most recent ones
    for (int i = DEBUG_INSTRUCTION_SIZE - 2; i >= 0; i--) {
        debug->instructions[i + 1] = debug->instructions[i];
    }
    debug->instructions[0] = instruction;
}

void debugger_reset(Debugger *debug) {
    memset(debug->instructions, 0, sizeof(debug->instructions));
}

void debugger_rom_load(Debugger *debug, size_t size) {
    debug->rom_size = size;
    snprintf((char *) debug->rom_loaded_message, 2048, "ROM loaded: %lu bytes loaded\n", size);
}

void debugger_rom_loaded_message_remove(Debugger *debug) {
    memset(debug->rom_loaded_message, 0, sizeof(debug->rom_loaded_message));
}

// END DEBUG UTILITIES

// INIT RAYLIB UTILITIES

Font debug_font;

void init_display() {
    InitWindow(
        DISPLAY_WIDTH * DISPLAY_SCALE + DEBUG_PANEL_WIDTH,
        DISPLAY_HEIGHT * DISPLAY_SCALE + DEBUG_PANEL_HEIGHT,
        "CHIP-8"
    );
    SetTargetFPS(60);

    debug_font = LoadFontEx("assets/JetBrainsMono-Bold.ttf", FONT_SIZE, NULL, 0);
}

bool quit_pressed() {
    return WindowShouldClose() || IsKeyPressed(KEY_CAPS_LOCK);;
}


void draw_text(const char *text, int32_t x, int32_t y, Color color) {
    DrawTextEx(
        debug_font,
        text,
        (Vector2){.x = (float) x, .y = (float) y},
        FONT_SIZE,
        0,
        color
    );
}

void draw_display(uint8_t *buffer, Debugger *debug) {
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
    draw_text(
        "INSTRUCTIONS\n",
        x_start,
        y_start,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    for (int i = 0; i < DEBUG_INSTRUCTION_SIZE; i++) {
        const char *format = "";

        if (debug->instructions[i]) {
            switch ((debug->instructions[i] & 0xF000) >> 12) {
                case 0x0:
                    format = "00E0 clear screen";
                    break;
                case 0x1:
                    format = "1NNN PC = NNN";
                    break;
                case 0x6:
                    format = "6XNN VX = NN";
                    break;
                case 0x7:
                    format = "7XNN VX += NN";
                    break;
                case 0xA:
                    format = "ANNN I = NNN";
                    break;
                case 0xD:
                    format = "DXYN sprite I on VX XY";
                    break;
                default:
                    format = "";
            }
        }

        if (i == 0) {
            if (debug->chip->halt) {
                format = "WRONG INSTRUCION";
            }
            draw_text(
                TextFormat("-> 0x%04X  %s\n", debug->instructions[i], format),
                x_start,
                y_start + FONT_SIZE * i,
                debug->chip->halt ? RED : GREEN
            );
        } else {
            draw_text(
                TextFormat("   0x%04X  %s\n", debug->instructions[i], format),
                x_start,
                y_start + FONT_SIZE * i,
                LIGHTGRAY
            );
        }
    }
    y_start += FONT_SIZE * DEBUG_INSTRUCTION_SIZE + MARGIN;

    draw_text(
        TextFormat("PC: 0x%08X\n", debug->chip->pc),
        x_start,
        y_start,
        SKYBLUE
    );
    y_start += FONT_SIZE + MARGIN;
    draw_text(
        TextFormat("RI: 0x%08X\n", debug->chip->register_i),
        x_start,
        y_start,
        GOLD
    );
    y_start += FONT_SIZE + MARGIN;

    // registers
    draw_text(
        "REGISTERS: R toggle hex-decimal view",
        x_start,
        y_start,
        LIGHTGRAY
    );
    y_start += FONT_SIZE;

    const char *register_format;
    if (debug->show_registers_decimal) {
        register_format = "V%X: %4d";
    } else {
        register_format = "V%X: 0x%02X";
    }

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 2; ++j) {
            draw_text(
                TextFormat(register_format, j * 8 + i, debug->chip->registers_v[j * 8 + i]),
                x_start + 100 * j,
                y_start + FONT_SIZE * i,
                LIGHTGRAY
            );
        }
    }
    y_start += FONT_SIZE * 8 + MARGIN * 5;

    // sprite of register I
    draw_text(
        "SPRITE in I (max height)",
        x_start,
        y_start,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    DrawRectangle(
        x_start,
        y_start,
        8 * DISPLAY_SCALE,
        SPRITE_MAX_HEIGHT * DISPLAY_SCALE,
        display_background
    );
    for (int y = 0; y < SPRITE_MAX_HEIGHT; y++) {
        uint8_t memory_byte = debug->chip->memory[debug->chip->register_i + y];
        for (int x = 0; x < 8; x++) {
            if (memory_byte >> (8 - 1 - x) & 0x01) {
                DrawRectangle(
                    x_start + x * DISPLAY_SCALE,
                    y_start + y * DISPLAY_SCALE,
                    DISPLAY_SCALE,
                    DISPLAY_SCALE,
                    LIGHTGRAY
                );
            }
        }
    }


    // memory layout
    x_start = MARGIN;
    y_start = DISPLAY_HEIGHT * DISPLAY_SCALE + MARGIN;

    draw_text(
        "MEMORY 0x",
        x_start,
        y_start,
        LIGHTGRAY
    );
    y_start += FONT_SIZE + MARGIN;

    int32_t rows = 15;
    int32_t cols = 16;

    x_start += 60;
    for (int i = 0; i < rows; i++) {
        // current memory address
        draw_text(
            TextFormat("%03X:", 0x200 + cols * i),
            x_start - 60,
            y_start + FONT_SIZE * i,
            LIGHTGRAY
        );

        for (int j = 0; j < cols; j++) {
            uint16_t address = 0x200 + (i * cols + j);
            Color color = LIGHTGRAY;

            if (address == debug->chip->register_i) {
                color = GOLD;
            }
            if (address == debug->chip->pc) {
                color = SKYBLUE;
            }
            if (address == debug->chip->register_i && address == debug->chip->pc) {
                color = GREEN;
            }
            draw_text(
                TextFormat("%02X\n", debug->chip->memory[address]),
                x_start + 40 * j,
                y_start + FONT_SIZE * i,
                color
            );
        }
    }
    y_start += FONT_SIZE * rows + MARGIN * 2;

    // messages
    draw_text(
        (char *) debug->rom_loaded_message,
        x_start,
        y_start,
        LIGHTGRAY
    );

    EndDrawing();
}

// END RAYLIB UTILITIES

int main(void) {
    Chip8 chip = {0};
    chip_reset(&chip);

    Debugger debugger = {0};
    debugger.chip = &chip;

    uint8_t display_buffer[DISPLAY_BUFFER_SIZE] = {0};
    init_display();

    bool is_executing = false;
    size_t message_timeout_s = 0;

    while (!quit_pressed()) {
        if (IsKeyPressed(KEY_ONE)) {
            size_t size = chip_load_rom(&chip, "data/1-chip8-logo.ch8");
            memset(display_buffer, 0, sizeof(display_buffer));
            debugger_reset(&debugger);
            debugger_rom_load(&debugger, size);
            message_timeout_s = 5 * 60;
        }
        if (IsKeyPressed(KEY_TWO)) {
            size_t size = chip_load_rom(&chip, "data/2-ibm-logo.ch8");
            memset(display_buffer, 0, sizeof(display_buffer));
            debugger_reset(&debugger);
            debugger_rom_load(&debugger, size);
            message_timeout_s = 5 * 60;
        }
        if (IsKeyPressed(KEY_THREE)) {
            size_t size = chip_load_rom(&chip, "data/3-corax+.ch8");
            memset(display_buffer, 0, sizeof(display_buffer));
            debugger_reset(&debugger);
            debugger_rom_load(&debugger, size);
            message_timeout_s = 5 * 60;
        }

        if (IsKeyPressed(KEY_C)) {
            // continue/stop execution
            is_executing = !is_executing;
        }

        if (IsKeyPressed(KEY_R)) {
            debugger.show_registers_decimal = !debugger.show_registers_decimal;
        }

        if (!chip.halt && (is_executing || IsKeyPressed(KEY_SPACE))) {
            chip_load_next_instruction(&chip);
            debugger_instruction_add(&debugger, chip.instruction);
            // decode
            uint8_t nibble_0 = (chip.instruction & 0xF000) >> 12;
            switch (nibble_0) {
                case 0x0: // clear screen
                    for (int i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
                        display_buffer[i] = 0;
                    }
                    break;
                case 0x1: // 1NNN jump
                    chip.pc = chip.instruction & 0x0FFF;
                    break;
                case 0x6: // 6XNN set register VX to NN
                    chip.registers_v[(chip.instruction & 0x0F00) >> 8] = chip.instruction & 0x00FF;
                    break;
                case 0x7: {
                    // 7XNN Add the value NN to VX
                    uint8_t x = chip.registers_v[(chip.instruction & 0x0F00) >> 8];
                    chip.registers_v[(chip.instruction & 0x0F00) >> 8] = x + (chip.instruction & 0x00FF);
                    break;
                }
                case 0xA: // ANNN set index register I to NNN
                    chip.register_i = chip.instruction & 0x0FFF;
                    break;
                case 0xD: {
                    // DXYN display
                    uint8_t x = chip.registers_v[(chip.instruction & 0x0F00) >> 8] % DISPLAY_WIDTH;
                    uint8_t y = chip.registers_v[(chip.instruction & 0x00F0) >> 4] % DISPLAY_HEIGHT;

                    chip.registers_v[0xF] = 0;
                    uint8_t n = chip.instruction & 0x000F;
                    for (int i = 0; i < n; i++) {
                        for (int j = 0; j < 8; j++) {
                            uint8_t screen_pixel = display_buffer[(y + i) * DISPLAY_WIDTH + x + j];
                            uint8_t sprite_pixel = (chip.memory[chip.register_i + i] >> (8 - 1 - j)) & 0x01;
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
                    chip.halt = true;
                    break;
            }
        }

        if (message_timeout_s > 0) {
            message_timeout_s--;
            if (!message_timeout_s) {
                debugger_rom_loaded_message_remove(&debugger);
            }
        }

        draw_display(display_buffer, &debugger);

        // limit to 700 CHIP-8 instructions per second
    }

    UnloadFont(debug_font);
    CloseWindow();
    return 0;
}
