#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "raylib.h"

#define MEM_SIZE 4096
#define REG_SIZE 16
#define STACK_SIZE 16
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_SCALE 15
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

#define DISPLAY_DEBUG_SCROLL_ZONE 0

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
    uint8_t registers_v[REG_SIZE]; // V0 - VF
    uint16_t stack_index;
    uint16_t stack[STACK_SIZE];
    uint8_t memory[MEM_SIZE];
    uint8_t display_buffer[DISPLAY_BUFFER_SIZE];
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

size_t chip_load_rom(Chip8 *chip, char *file_path) {
    memset(chip, 0, sizeof(Chip8));
    chip_load_font(chip);
    chip->pc = 0x200;

    FILE *rom = fopen(file_path, "rb");
    if (rom == NULL) {
        fprintf(stderr, "Could not open ROM file\n");
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_END);
    size_t size = ftell(rom);
    if (size > MEM_SIZE - 0x200) {
        fprintf(stderr, "ROM is too large to fit in memory (%zu bytes, max %d)\n", size, MEM_SIZE - 0x200);
        exit(EXIT_FAILURE);
    }
    fseek(rom, 0, SEEK_SET);
    fread(chip->memory + 0x200, sizeof(chip->memory[0]), size, rom);
    fclose(rom);
    return size;
}

void chip_stack_push(Chip8 *chip, uint16_t address) {
    assert(chip->stack_index < STACK_SIZE);
    chip->stack[chip->stack_index++] = address;
}

uint16_t chip_stack_pop(Chip8 *chip) {
    assert(chip->stack_index > 0);
    uint16_t address = chip->stack[--chip->stack_index];
    chip->stack[chip->stack_index] = 0;
    return address;
}

void chip_execute_next_instruction(Chip8 *chip) {
    if (chip->halt) {
        return;
    }
    // fetch instruction from memory at pc address
    chip->instruction = (chip->memory[chip->pc] << 8) | chip->memory[chip->pc + 1];
    chip->pc += 2;

    uint8_t nibble_0 = (chip->instruction & 0xF000) >> 12;
    uint8_t nibble_1 = (chip->instruction & 0x0F00) >> 8;
    uint8_t nibble_2 = (chip->instruction & 0x00F0) >> 4;
    uint8_t nibble_3 = chip->instruction & 0x000F;
    switch (nibble_0) {
        case 0x0:
            if (nibble_3 == 0x0) {
                // 00E0 clear screen
                for (int i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
                    chip->display_buffer[i] = 0;
                }
            } else if (nibble_3 == 0xE) {
                // 00EE return from subroutine
                chip->pc = chip_stack_pop(chip);
            } else {
                chip->halt = true;
            }
            break;
        case 0x1: // 1NNN jump
            chip->pc = chip->instruction & 0x0FFF;
            break;
        case 0x2: // 2NNN subroutine
            chip_stack_push(chip, chip->pc);
            chip->pc = chip->instruction & 0x0FFF;
            break;
        case 0x3: // 3XNN skip 1 instruction if VX = NN
            if (chip->registers_v[nibble_1] == (chip->instruction & 0x00FF)) {
                chip->pc += 2;
            }
            break;
        case 0x4: // 4XNN skip 1 instruction if VX != NN
            if (chip->registers_v[nibble_1] != (chip->instruction & 0x00FF)) {
                chip->pc += 2;
            }
            break;
        case 0x5: // 5XY0 skip 1 instruction if VX == XY
            if (chip->registers_v[nibble_1] == chip->registers_v[nibble_2]) {
                chip->pc += 2;
            }
            break;
        case 0x6: // 6XNN set register VX to NN
            chip->registers_v[nibble_1] = chip->instruction & 0x00FF;
            break;
        case 0x7: {
            // 7XNN Add the value NN to VX
            uint8_t x = chip->registers_v[nibble_1];
            chip->registers_v[nibble_1] = x + (chip->instruction & 0x00FF);
            break;
        }
        case 0x8: // 8XXX
            switch (nibble_3) {
                case 0x0: // 8XY0 VX = VY
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_2];
                    break;
                case 0x1: // 8XY1 VX |= VY
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_1] | chip->registers_v[nibble_2];
                    break;
                case 0x2: // 8XY2 VX &= VY
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_1] & chip->registers_v[nibble_2];
                    break;
                case 0x3: // 8XY3 VX ^= VY
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_1] ^ chip->registers_v[nibble_2];
                    break;
                case 0x4: // 8XY4 VX += VY, VF = carry
                    chip->registers_v[0xF] = chip->registers_v[nibble_1] + chip->registers_v[nibble_2] > UINT8_MAX;
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_1] + chip->registers_v[nibble_2];
                    break;
                case 0x5: // 8XY5 VX -= VY, VF = !borrow
                    chip->registers_v[0xF] = chip->registers_v[nibble_1] >= chip->registers_v[nibble_2];
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_1] - chip->registers_v[nibble_2];
                    break;
                case 0x6: // 8XY6 VX = VY >> 1, VF = shifted bit
                    chip->registers_v[0xF] = chip->registers_v[nibble_2] & 0x1;
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_2] >> 1;
                    break;
                case 0x7: // 8XY7 VX = VY - VX, VF = !borrow
                    chip->registers_v[0xF] =  chip->registers_v[nibble_2] >= chip->registers_v[nibble_1];
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_2] - chip->registers_v[nibble_1];
                    break;
                case 0xE: // 8XYE VX = VY << 1, VF = shifted bit
                    chip->registers_v[0xF] = chip->registers_v[nibble_2] >> 7;
                    chip->registers_v[nibble_1] = chip->registers_v[nibble_2] << 1;
                    break;
                default:
                    chip->halt = true;
                    break;
            }
            break;
        case 0x9: // 9XY0 skip 1 instruction if VX != XY
            if (chip->registers_v[nibble_1] != chip->registers_v[nibble_2]) {
                chip->pc += 2;
            }
            break;
        case 0xA: // ANNN set index register I to NNN
            chip->register_i = chip->instruction & 0x0FFF;
            break;
        case 0xD: {
            // DXYN display
            uint8_t x = chip->registers_v[nibble_1] % DISPLAY_WIDTH;
            uint8_t y = chip->registers_v[(chip->instruction & 0x00F0) >> 4] % DISPLAY_HEIGHT;

            chip->registers_v[0xF] = 0;
            for (int i = 0; i < nibble_3; i++) {
                for (int j = 0; j < 8; j++) {
                    uint8_t screen_pixel = chip->display_buffer[(y + i) * DISPLAY_WIDTH + x + j];
                    uint8_t sprite_pixel = (chip->memory[chip->register_i + i] >> (8 - 1 - j)) & 0x01;
                    if (sprite_pixel && screen_pixel) {
                        chip->display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 0;
                        chip->registers_v[0xF] = 1;
                    }
                    if (sprite_pixel && !screen_pixel) {
                        chip->display_buffer[(y + i) * DISPLAY_WIDTH + x + j] = 1;
                        chip->registers_v[0xF] = 1;
                    }
                }
            }
            break;
        }
        default:
            chip->halt = true;
            break;
    }
}

// END EMULATOR

// INIT DEBUG UTILITIES
#define DEBUG_INSTRUCTION_SIZE 8
#define DEBUG_ROM_MESSAGE_LIMIT 512

typedef struct {
    bool show_registers_decimal;
    bool last_touched_registers[16];
    size_t rom_size;
    char rom_loaded_message[DEBUG_ROM_MESSAGE_LIMIT];
    uint16_t instructions[DEBUG_INSTRUCTION_SIZE];

    bool is_executing; // whether the chip is executing or in pause
    uint16_t breakpoint; // 0 = no breakpoint for easy {0} init

    // mem visualizer starting point
    int16_t mem_start;

    Chip8 *chip;
} Debugger;

Debugger debugger_init(Chip8 *chip) {
    Debugger dbg = {0};
    dbg.chip = chip;
    dbg.mem_start = 0x200;
    return dbg;
}

void debugger_instruction_add(Debugger *dbg, uint16_t instruction) {
    // Show current instruction and the 9 most recent ones
    for (int i = DEBUG_INSTRUCTION_SIZE - 2; i >= 0; i--) {
        dbg->instructions[i + 1] = dbg->instructions[i];
    }
    dbg->instructions[0] = instruction;
}

void debugger_next(Debugger *dbg, bool toggle_play_pause, bool step_once) {
    if (dbg->chip->halt) {
        dbg->is_executing = false;
        return;
    }
    if (dbg->breakpoint == dbg->chip->pc) {
        dbg->is_executing = false;
    }
    if (toggle_play_pause) {
        // continue/stop execution
        dbg->is_executing = !dbg->is_executing;
    }

    bool next_step = dbg->is_executing || step_once;
    if (next_step) {
        uint8_t previous_registers[REG_SIZE] = {0};
        memcpy(previous_registers, dbg->chip->registers_v, sizeof(dbg->chip->registers_v));

        chip_execute_next_instruction(dbg->chip);
        debugger_instruction_add(dbg, dbg->chip->instruction);

        for (int i = 0; i < REG_SIZE; i++) {
            dbg->last_touched_registers[i] = previous_registers[i] != dbg->chip->registers_v[i];
        }
    }
}

void debugger_reset(Debugger *dbg) {
    memset(dbg->instructions, 0, sizeof(dbg->instructions));
    memset(dbg->last_touched_registers, 0, sizeof(dbg->last_touched_registers));
}

void debugger_rom_load(Debugger *dbg, char *file_path) {
    dbg->rom_size = chip_load_rom(dbg->chip, file_path);
    snprintf(dbg->rom_loaded_message, DEBUG_ROM_MESSAGE_LIMIT, "ROM loaded: %zu bytes loaded\n", dbg->rom_size);

    debugger_reset(dbg);
}

void debugger_rom_loaded_message_remove(Debugger *dbg) {
    memset(dbg->rom_loaded_message, 0, sizeof(dbg->rom_loaded_message));
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

void draw_display(uint8_t *buffer, Debugger *dbg) {
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

        uint8_t nibble3 = dbg->instructions[i] & 0x000F;

        if (dbg->instructions[i]) {
            switch ((dbg->instructions[i] & 0xF000) >> 12) {
                case 0x0:
                    if (nibble3 == 0x0) {
                        format = "00E0 clear screen";
                    } else if (nibble3 == 0xE) {
                        format = "00EE return from subroutine";
                    }
                    break;
                case 0x1:
                    format = "1NNN PC = NNN";
                    break;
                case 0x2:
                    format = "2NNN call subroutine";
                    break;
                case 0x3:
                    format = "3XNN skip 1 instruction if VX = NN";
                    break;
                case 0x4:
                    format = "4XNN skip 1 instruction if VX != NN";
                    break;
                case 0x5:
                    format = "5XY0 skip 1 instruction if VX == VY";
                    break;
                case 0x6:
                    format = "6XNN VX = NN";
                    break;
                case 0x7:
                    format = "7XNN VX += NN";
                    break;
                    case 0x8:
                    switch (nibble3) {
                        case 0x0:
                            format = "8XY0 VX = VY";
                            break;
                        case 0x1:
                            format = "8XY1 VX |= VY";
                            break;
                        case 0x2:
                            format = "8XY2 VX &= VY";
                            break;
                        case 0x3:
                            format = "8XY3 VX ^= VY";
                            break;
                        case 0x4:
                            format = "8XY4 VX += VY";
                            break;
                        case 0x5:
                            format = "8XY5 VX -= VY";
                            break;
                        case 0x6:
                            format = "8XY6 VX = VY >> 1";
                            break;
                        case 0x7:
                            format = "8XY7 VX = VY - VX";
                            break;
                        case 0xE:
                            format = "8XYE VX = VY << 1";
                            break;
                        default:
                            format = "";
                    }
                    break;
                case 0x9:
                    format = "9XY0 skip 1 instruction if VX != VY";
                    break;
                case 0xA:
                    format = "ANNN I = NNN";
                    break;
                case 0xD:
                    format = "DXYN draw on VX VY sprite in I address";
                    break;
                default:
                    format = "";
            }
        }

        if (i == 0) {
            if (dbg->chip->halt) {
                format = "WRONG INSTRUCION";
            }
            draw_text(
                TextFormat("-> 0x%04X  %s\n", dbg->instructions[i], format),
                x_start,
                y_start + FONT_SIZE * i,
                dbg->chip->halt ? RED : GREEN
            );
        } else {
            draw_text(
                TextFormat("   0x%04X  %s\n", dbg->instructions[i], format),
                x_start,
                y_start + FONT_SIZE * i,
                LIGHTGRAY
            );
        }
    }
    y_start += FONT_SIZE * DEBUG_INSTRUCTION_SIZE + MARGIN;

    draw_text(
        TextFormat("PC: 0x%08X\n", dbg->chip->pc),
        x_start,
        y_start,
        SKYBLUE
    );
    y_start += FONT_SIZE + MARGIN;
    draw_text(
        TextFormat("RI: 0x%08X\n", dbg->chip->register_i),
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
    if (dbg->show_registers_decimal) {
        register_format = "V%X: %4d";
    } else {
        register_format = "V%X: 0x%02X";
    }

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 2; ++j) {
            draw_text(
                TextFormat(register_format, j * 8 + i, dbg->chip->registers_v[j * 8 + i]),
                x_start + 100 * j,
                y_start + FONT_SIZE * i,
                dbg->last_touched_registers[j * 8 + i] ? GREEN : LIGHTGRAY
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
        uint8_t memory_byte = dbg->chip->memory[dbg->chip->register_i + y];
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
    int32_t mem_width_cell = 40;
    int32_t mem_label_space = 60;

    // SCROLL MEMORY
    Rectangle scroll_region = {
        .x = (float) x_start,
        .y = (float) y_start,
        .width = (float) mem_width_cell * (float) cols + (float) mem_label_space,
        .height = FONT_SIZE * rows
    };
    if (DISPLAY_DEBUG_SCROLL_ZONE) {
        DrawRectangle(
            x_start,
            y_start,
            mem_width_cell * cols + mem_label_space,
            FONT_SIZE * rows,
            DARKPURPLE
        );
    }
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(GetMousePosition(), scroll_region) && wheel != 0) {
        if (wheel < 0) {
            if (dbg->mem_start < MEM_SIZE - rows * cols) {
                dbg->mem_start += rows + 1;
            }
        } else {
            if (dbg->mem_start > 0x200) {
                dbg->mem_start -= rows + 1;
            }
        }
    }

    // DRAW MEMORY
    x_start += mem_label_space;
    for (int i = 0; i < rows; i++) {
        // current memory address
        draw_text(
            TextFormat("%03X:", dbg->mem_start + cols * i),
            x_start - mem_label_space,
            y_start + FONT_SIZE * i,
            LIGHTGRAY
        );

        for (int j = 0; j < cols; j++) {
            uint16_t address = dbg->mem_start + (i * cols + j);
            Color color = LIGHTGRAY;

            if (dbg->breakpoint == address) {
                DrawRectangle(
                    x_start + mem_width_cell * j,
                    y_start + FONT_SIZE * i,
                    mem_width_cell + mem_width_cell / 2,
                    mem_width_cell / 2,
                    RED
                );
            }

            if (address == dbg->chip->register_i) {
                color = GOLD;
            }
            if (address == dbg->chip->pc) {
                color = SKYBLUE;
            }
            if (address == dbg->chip->register_i && address == dbg->chip->pc) {
                color = GREEN;
            }
            draw_text(
                TextFormat("%02X\n", dbg->chip->memory[address]),
                x_start + mem_width_cell * j,
                y_start + FONT_SIZE * i,
                color
            );

            // set breakpoint
            Rectangle cell = {
                .x = (float) x_start + (float) mem_width_cell * j,
                .y = (float) y_start + FONT_SIZE * i,
                .width = (float) mem_width_cell,
                .height = FONT_SIZE
            };
            if (CheckCollisionPointRec(GetMousePosition(), cell) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // pc only ever lands on even addrs (0x200 start, +2 per step)
                // mask with xxx0 so the address always end in 0 thus even
                uint16_t pc_aligned_address = address & ~1u;

                if (dbg->breakpoint == pc_aligned_address) {
                    dbg->breakpoint = 0;
                } else {
                    dbg->breakpoint = pc_aligned_address;
                }
            }
        }
    }
    y_start += FONT_SIZE * rows + MARGIN * 2;

    // messages
    draw_text(
        dbg->rom_loaded_message,
        x_start,
        y_start,
        LIGHTGRAY
    );

    EndDrawing();
}

// END RAYLIB UTILITIES

int main(void) {
    size_t message_timeout_s = 0;

    Chip8 chip = {0};
    Debugger dbg = debugger_init(&chip);

    // temporal for speeed of debugging, remove at some point
    debugger_rom_load(&dbg, "data/3-corax+.ch8");

    init_display();

    while (!quit_pressed()) {
        if (IsKeyPressed(KEY_ONE)) {
            debugger_rom_load(&dbg, "data/1-chip8-logo.ch8");
            message_timeout_s = 5 * 60;
        }
        if (IsKeyPressed(KEY_TWO)) {
            debugger_rom_load(&dbg, "data/2-ibm-logo.ch8");
            message_timeout_s = 5 * 60;
        }
        if (IsKeyPressed(KEY_THREE)) {
            debugger_rom_load(&dbg, "data/3-corax+.ch8");
            message_timeout_s = 5 * 60;
        }

        if (IsKeyPressed(KEY_R)) {
            dbg.show_registers_decimal = !dbg.show_registers_decimal;
        }

        debugger_next(&dbg, IsKeyPressed(KEY_C), IsKeyPressed(KEY_SPACE));

        if (message_timeout_s > 0) {
            message_timeout_s--;
            if (!message_timeout_s) {
                debugger_rom_loaded_message_remove(&dbg);
            }
        }

        draw_display(dbg.chip->display_buffer, &dbg);

        // limit to 700 CHIP-8 instructions per second
    }

    UnloadFont(debug_font);
    CloseWindow();
    return EXIT_SUCCESS;
}
