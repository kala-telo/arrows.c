#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#define RAYGUI_CUSTOM_ICONS
#include "icons.h"
#include "raygui.h"
#include "style_dark.h"
#include <omp.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define max(x, y) ((x) > (y) ? (x) : (y))

#define UNLOAD_TIMER_MAX 5
#define MAX_TPS 10000
#define UI_BACKGROUND_ALPHA 0.9f

#define CHUNK_SIZE 16
#define CAMERA_SPPED 2.0f

#define DEFAUL_ZOOM 16

enum CellType {
    Empty,
    Arrow,
    Source,
    Blocker,
    Delay,
    Detector,
    SplitterUpDown,
    SplitterUpRight,
    SplitterUpRightLeft,
    Pulse,
    BlueArrow,
    Diagonal,
    BlueSplitterUpUp,
    BlueSplitterRightUp,
    BlueSplitterUpDiagonal,
    Not,
    And,
    Xor,
    Latch,
    Flipflop,
    Random,
    Button,
    LevelSource,
    LevelTarget,
    DirectoinalButton,
};

enum SignalType {
    S_NONE,
    S_BLOCK,
    S_RED,
    S_BLUE,
    S_YELLOW,
    S_ORANGE,
    S_DELAY_AFTER_RED,
};

enum Direction {
    D_NORTH,
    D_EAST,
    D_SOUTH,
    D_WEST,
};

typedef struct {
    enum CellType type;
    // XXX: second signal counter
    int signal_count;
    enum SignalType signal;
    enum Direction direction;
    bool flipped;
} arrow_t;

struct ChunkPos {int16_t x; int16_t y; };
typedef struct {
    int32_t x;
    int32_t y;
} point_t;

typedef struct {
    enum QueueType {
        QT_POWER,
        QT_BLOCK
    } type;
    point_t position;
} queue_t;

typedef struct {
    arrow_t arrows[256];
     queue_t *update_queue;
    uint8_t unload_timer;
} chunk_t;
typedef struct map_t {
    uint16_t version;
    struct HashTable { struct ChunkPos key; chunk_t value; } *chunks;
} map_t;

static inline struct ChunkPos pos2chunk(int x, int y) {
    return (struct ChunkPos){
        .x = x / CHUNK_SIZE,
        .y = y / CHUNK_SIZE
    };
}

static inline void chunk2pos(uint32_t pos, int16_t *x, int16_t *y) {
    assert(x != NULL);
    assert(y != NULL);
    uint16_t ux = (pos & 0xFF00) >> 16;
    *x = *(int16_t*)&ux;
    uint16_t uy = (pos & 0xFF);
    *y = *(int16_t*)&uy;
}

void map_init(map_t *map) {
    // TODO: map destroy
}

arrow_t* map_get(map_t *map, int32_t x, int32_t y) {
    struct HashTable *kv = hmgetp_null(map->chunks, pos2chunk(x, y));
    if (kv == NULL) {
        hmput(map->chunks, pos2chunk(x, y), (chunk_t){ 0 });
        kv = hmgetp_null(map->chunks, pos2chunk(x, y));
    }
    chunk_t *chunk = &kv->value;
    x %= CHUNK_SIZE;
    y %= CHUNK_SIZE;
    return &chunk->arrows[y*CHUNK_SIZE+x];
}

void map_power(chunk_t *chunk, int x, int y) {
    arrput(chunk->update_queue, ((queue_t){ .type = QT_POWER, .position = (point_t){.x = x, .y = y }}));
}

void map_block(chunk_t *chunk, int x, int y) {
    arrput(chunk->update_queue, ((queue_t){ .type = QT_BLOCK, .position = (point_t){.x = x, .y = y }}));
}

void map_queue_update(map_t *map) {
    for(size_t chunk_i = 0; chunk_i < hmlenu(map->chunks); chunk_i++) {
        const size_t qlen = arrlenu(map->chunks[chunk_i].value.update_queue);
        arrsetlen(map->chunks[chunk_i].value.update_queue, 0);
        for(size_t i = 0; i < qlen; i++) {
            queue_t q = map->chunks[chunk_i].value.update_queue[i];
            hmgetp(map->chunks, pos2chunk(q.position.x, q.position.y))->value.unload_timer = 0;
            arrow_t *arrow = map_get(map, q.position.x, q.position.y);
            switch (q.type) {
            case QT_BLOCK:
                arrow->signal = S_BLOCK;
                arrow->signal_count = 0;
                break;
            case QT_POWER:
                if (arrow->signal == S_BLOCK) break;
                arrow->signal_count++;
                switch(arrow->type) {
                case Empty:
                    break;
                case Arrow:
                    arrow->signal = S_RED;
                    break;
                case Source:
                    break;
                case Blocker:
                    arrow->signal = S_RED;
                    break;
                case Delay:
                    if (arrow->signal == S_NONE || arrow->signal == S_BLOCK)
                        arrow->signal = S_BLUE;
                    else
                        arrow->signal = S_RED;
                    break;
                case Detector:
                    break;
                case SplitterUpDown:
                    arrow->signal = S_RED;
                    break;
                case SplitterUpRight:
                    arrow->signal = S_RED;
                    break;
                case SplitterUpRightLeft:
                    arrow->signal = S_RED;
                    break;
                case Pulse:
                    break;
                case BlueArrow:
                    arrow->signal = S_BLUE;
                    break;
                case Diagonal:
                    arrow->signal = S_BLUE;
                    break;
                case BlueSplitterUpUp:
                    arrow->signal = S_BLUE;
                    break;
                case BlueSplitterRightUp:
                    arrow->signal = S_BLUE;
                    break;
                case BlueSplitterUpDiagonal:
                    arrow->signal = S_BLUE;
                    break;
                case Not:
                    arrow->signal = S_NONE;
                    break;
                case And:
                    arrow->signal = arrow->signal_count >= 2 ? S_YELLOW : S_NONE;
                    break;
                case Xor:
                    arrow->signal = arrow->signal_count % 2 != 0 ? S_YELLOW : S_NONE;
                    break;
                case Latch:
                    arrow->signal = arrow->signal_count >= 2 ? S_YELLOW : S_NONE;
                    break;
                case Flipflop:
                    if(arrow->signal_count > 1) { break; }
                    arrow->signal = arrow->signal == S_NONE ? S_YELLOW : S_NONE;
                    break;
                case Random:
                    arrow->signal = rand() % 2;
                    break;
                case Button:
                    break;
                case LevelSource:
                    break;
                case LevelTarget:
                    break;
                case DirectoinalButton:
                    arrow->signal = S_ORANGE;
                    break;
                }
                break;
            }
        }
    }
}

void map_import(map_t *map, const char *input) {
    // FIXME: unsafe
    uint8_t *buffer = malloc(strlen(input));
    assert(buffer != NULL && "No RAM");
    size_t buffer_size = 0;
    {
        // FIXME: unsafe
        size_t length = strlen(input);
        const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (size_t i = 0; i < length; i += 4) {
            uint8_t sextet[4];

            for (size_t j = 0; j < 4; j++) {
                sextet[j] = strchr(base64chars, input[i + j]) - base64chars;
            }

            uint32_t decoded_value = (sextet[0] << 18) | (sextet[1] << 12) | (sextet[2] << 6) | sextet[3];

            for (int j = 2; j >= 0; j--) {
                if (i + j < length - 1) {
                    char decoded_char = (decoded_value >> (j * 8)) & 0xFF;
                    buffer[buffer_size++] = decoded_char;
                }
            }
        }
    }

    size_t buffer_index = 0;
#define pop8() ({ \
        assert(buffer_index < buffer_size); \
        uint8_t value = buffer[buffer_index]; \
        buffer_index++; \
        value; })
#define pop16()  ({ \
        assert(buffer_index < buffer_size); \
        uint16_t value = *((uint16_t*)(buffer + buffer_index)); \
        buffer_index += 2; \
        value; })

    map->version = pop16();
    uint16_t chunk_count = pop16();
    for (uint16_t _chunk = 0; _chunk < chunk_count; _chunk++) {
        int32_t chunk_x = pop16();
        int32_t chunk_y = pop16();
        uint8_t types_count = pop8() + 1;
        for(uint8_t _type = 0; _type < types_count; _type++) {
            uint8_t arrow_type = pop8();
            uint8_t arrow_count = pop8() + 1;
            for(uint8_t _arrow = 0; _arrow < arrow_count; _arrow++) {
                uint8_t position = pop8();
                int32_t arrow_y = (position & 0xF0) >> 4;
                int32_t arrow_x = (position & 0x0F) >> 0;
                arrow_x += CHUNK_SIZE*chunk_x;
                arrow_y += CHUNK_SIZE*chunk_y;
                arrow_t *arrow = map_get(map, arrow_x, arrow_y);
                uint8_t direction_and_flip = pop8();
                arrow->direction = direction_and_flip & 0b11;
                arrow->flipped = (direction_and_flip & 0b100) != 0;
                arrow->type = arrow_type;
            }
        }
    }

#undef pop8
#undef pop16
    free(buffer);
}

void map_update(map_t *map) {
    const size_t chunk_count = hmlen(map->chunks);
    #pragma omp parallel for
    for(size_t i = 0; i < chunk_count; i++) {
        chunk_t *chunk = &map->chunks[i].value;
        if (chunk->unload_timer >= UNLOAD_TIMER_MAX)
            continue;
        chunk->unload_timer++;
        int16_t  chunk_x = map->chunks[i].key.x,
                 chunk_y = map->chunks[i].key.y;

        // XXX: DEBUG
        /* DrawRectangle( */
        /*         (chunk_x*CHUNK_SIZE+OFFSET_X)*ZOOM-ZOOM/2, */
        /*         (chunk_y*CHUNK_SIZE+OFFSET_Y)*ZOOM-ZOOM/2, */
        /*         CHUNK_SIZE*ZOOM, CHUNK_SIZE*ZOOM, ColorAlpha(PURPLE, 0.5)); */
        /* char buffer[10]; */
        /* sprintf(buffer, "%d", chunk->unload_timer); */
        /* DrawText(buffer, (chunk_x*CHUNK_SIZE+OFFSET_X)*ZOOM-ZOOM/2, */
        /*          (chunk_y*CHUNK_SIZE+OFFSET_Y)*ZOOM-ZOOM/2, 10, BLACK); */
        // XXX: DEBUG
        static const int updates_straight[4][2] = {
            [D_NORTH]  = { 0, -1},
            [D_EAST]   = { 1,  0},
            [D_SOUTH]  = { 0,  1},
            [D_WEST]   = {-1,  0},
        };
        static const int updates_diagonal[4][2] = {
            [D_NORTH]  = { 1, -1},
            [D_EAST]   = { 1,  1},
            [D_SOUTH]  = {-1,  1},
            [D_WEST]   = {-1, -1},
        };
        for(size_t j = 0; j < CHUNK_SIZE*CHUNK_SIZE; j++) {
            int arrow_x = chunk_x * CHUNK_SIZE + j % CHUNK_SIZE;
            int arrow_y = chunk_y * CHUNK_SIZE + j / CHUNK_SIZE;
            arrow_t *arrow = &chunk->arrows[j];
            switch(arrow->type) {
                case Empty:
                    break;
                case Arrow:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    arrow->signal = S_NONE;
                    break;
                case Source:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_RED;
                        break;
                    }
                    map_power(chunk, arrow_x+1, arrow_y);
                    map_power(chunk, arrow_x-1, arrow_y);
                    map_power(chunk, arrow_x  , arrow_y+1);
                    map_power(chunk, arrow_x  , arrow_y-1);
                    break;
                case Blocker:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_block(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    arrow->signal = S_NONE;
                    break;
                case Delay:
                    switch(arrow->signal) {
                        case S_BLOCK:
                        case S_DELAY_AFTER_RED:
                            arrow->signal = S_NONE;
                            break;
                        case S_NONE:
                            break;
                        case S_BLUE:
                            arrow->signal = S_RED;
                            break;
                        case S_RED:
                            map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                            arrow->signal = S_DELAY_AFTER_RED;
                            break;
                        case S_YELLOW:
                        case S_ORANGE:
                            break;
                    }
                    break;
                case Detector:
                    assert(false && "Detector is even more evil than Blocker");
                    break;
                case SplitterUpDown:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    map_power(chunk, arrow_x+updates_straight[(arrow->direction + 2) % 4][0], arrow_y + updates_straight[(arrow->direction + 2) % 4][1]);
                    arrow->signal = S_NONE;
                    break;
                case SplitterUpRight:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    if (arrow->flipped) {
                        map_power(chunk, arrow_x+updates_straight[(arrow->direction + 3) % 4][0], arrow_y + updates_straight[(arrow->direction + 3) % 4][1]);
                    } else {
                        map_power(chunk, arrow_x+updates_straight[(arrow->direction + 1) % 4][0], arrow_y + updates_straight[(arrow->direction + 1) % 4][1]);
                    }
                    arrow->signal = S_NONE;
                    break;
                case SplitterUpRightLeft:
                    if(arrow->signal != S_RED) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    map_power(chunk, arrow_x+updates_straight[(arrow->direction + 1) % 4][0], arrow_y + updates_straight[(arrow->direction + 1) % 4][1]);
                    map_power(chunk, arrow_x+updates_straight[(arrow->direction + 3) % 4][0], arrow_y + updates_straight[(arrow->direction + 3) % 4][1]);
                    arrow->signal = S_NONE;
                    break;
                case Pulse:
                    switch(arrow->signal) {
                        case S_BLOCK:
                        case S_DELAY_AFTER_RED: // Should be unreachable
                        case S_NONE:
                            arrow->signal = S_RED;
                            break;
                        case S_BLUE:
                            break;
                        case S_RED:
                            map_power(chunk, arrow_x+1, arrow_y);
                            map_power(chunk, arrow_x-1, arrow_y);
                            map_power(chunk, arrow_x, arrow_y+1);
                            map_power(chunk, arrow_x, arrow_y-1);
                            arrow->signal = S_BLUE;
                            break;
                        case S_YELLOW:
                        case S_ORANGE:
                            break;
                    }
                    break;
                case BlueArrow:
                    if(arrow->signal != S_BLUE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0]*2, arrow_y+updates_straight[arrow->direction][1]*2);
                    arrow->signal = S_NONE;
                    break;
                case Diagonal:
                    if(arrow->signal != S_BLUE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    if(arrow->flipped) {
                        map_power(chunk, arrow_x+updates_diagonal[(arrow->direction+3)%4][0], arrow_y + updates_diagonal[(arrow->direction+3)%4][1]);
                    } else {
                        map_power(chunk, arrow_x+updates_diagonal[arrow->direction][0], arrow_y + updates_diagonal[arrow->direction][1]);
                    }
                    arrow->signal = S_NONE;
                    break;
                case BlueSplitterUpUp:
                    if(arrow->signal != S_BLUE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0]*2, arrow_y + updates_straight[arrow->direction][1]*2);
                    arrow->signal = S_NONE;
                    break;
                case BlueSplitterRightUp:
                    if(arrow->signal != S_BLUE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[(arrow->direction) % 4][0]*2, arrow_y + updates_straight[(arrow->direction) % 4][1]*2);
                    if (arrow->flipped) {
                        map_power(chunk, arrow_x+updates_straight[(arrow->direction + 3) % 4][0], arrow_y + updates_straight[(arrow->direction + 3) % 4][1]);
                    } else {
                        map_power(chunk, arrow_x+updates_straight[(arrow->direction + 1) % 4][0], arrow_y + updates_straight[(arrow->direction + 1) % 4][1]);
                    }
                    arrow->signal = S_NONE;
                    break;
                case BlueSplitterUpDiagonal:
                    if(arrow->signal != S_BLUE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    if(arrow->flipped) {
                        map_power(chunk, arrow_x+updates_diagonal[(arrow->direction+3)%4][0], arrow_y + updates_diagonal[(arrow->direction+3)%4][1]);
                    } else {
                        map_power(chunk, arrow_x+updates_diagonal[arrow->direction][0], arrow_y + updates_diagonal[arrow->direction][1]);
                    }
                    arrow->signal = S_NONE;
                    break;
                case Not:
                    if(arrow->signal != S_YELLOW) {
                        arrow->signal = S_YELLOW;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y+updates_straight[arrow->direction][1]);
                    break;
                case And:
                case Xor:
                    if(arrow->signal != S_YELLOW) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    arrow->signal = S_NONE;
                    break;
                case Latch:
                case Flipflop:
                    if(arrow->signal != S_YELLOW) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    break;
                case Random:
                case DirectoinalButton:
                case Button:
                    if(arrow->signal != S_ORANGE) {
                        arrow->signal = S_NONE;
                        break;
                    }
                    map_power(chunk, arrow_x+updates_straight[arrow->direction][0], arrow_y + updates_straight[arrow->direction][1]);
                    arrow->signal = S_NONE;
                    break;

                case LevelSource:
                    break;
                case LevelTarget: break;
            }
            arrow->signal_count = 0;
        }
    }
#pragma omp critical
    map_queue_update(map);
}

#define DARK_RED    (Color){0x99, 0x00, 0x00, 0xff}
#define DARK_BLUE   (Color){0x00, 0x00, 0x66, 0xff}
#define DARK_ORANGE (Color){0x99, 0x4C, 0x00, 0xff}
#define DARK_YELLOW (Color){0x99, 0x66, 0x00, 0xff}

enum INPUT_EVENT {
    ARROW_SET,
    ARROW_NORTH,
    ARROW_SOUTH,
    ARROW_WEST,
    ARROW_EAST,
    ARROW_FLIP,
    ARROW_INTERACT,
    ARROW_REMOVE,
    ARROW_MENU,
    SELECT,
    SELECTED_REMOVE,
    SELECTED_COPY,
    CLIPBOARD_PASTE,
    HOTBAR1,
    HOTBAR2,
    HOTBAR3,
    HOTBAR4,
    HOTBAR5,
    HOTBAR_DESELECT,
    HOTBAR_PREVIOUS,
    HOTBAR_NEXT,
    PIPETTE,
    MOVE,
    MOVE_FORWARD,
    MOVE_BACK,
    MOVE_LEFT,
    MOVE_RIGHT,
    CENTER,
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_RESET,
    SIGNALS_REMOVE,
    PAUSE,
    MAP_MENU
};

KeyboardKey keybindings[] = {
    [ARROW_NORTH] = KEY_W,
    [ARROW_SOUTH] = KEY_S,
    [ARROW_WEST] = KEY_A,
    [ARROW_EAST] = KEY_D,
    [ARROW_FLIP] = KEY_F,
    [ARROW_REMOVE] = KEY_R,
    [ARROW_MENU] = KEY_TAB,
    [SELECT] = KEY_E,
    [SELECTED_REMOVE] = KEY_BACKSPACE,
    [SELECTED_COPY] = KEY_C,
    [CLIPBOARD_PASTE] = KEY_V,
    [HOTBAR1] = KEY_ONE,
    [HOTBAR2] = KEY_TWO,
    [HOTBAR3] = KEY_THREE,
    [HOTBAR4] = KEY_FOUR,
    [HOTBAR5] = KEY_FIVE,
    [HOTBAR_DESELECT] = KEY_GRAVE,
    [HOTBAR_PREVIOUS] = KEY_Z,
    [HOTBAR_NEXT] = KEY_X,
    [PIPETTE] = KEY_Q,
    [MOVE_FORWARD] = KEY_UP,
    [MOVE_BACK] = KEY_DOWN,
    [MOVE_LEFT] = KEY_LEFT,
    [MOVE_RIGHT] = KEY_RIGHT,
    [CENTER] = KEY_H,
    [ZOOM_IN] = KEY_EQUAL,
    [ZOOM_OUT] = KEY_MINUS,
    [ZOOM_RESET] = KEY_EQUAL,
    [SIGNALS_REMOVE] = KEY_N,
    [PAUSE] = KEY_SPACE,
    [MAP_MENU] = KEY_ESCAPE
};
MouseButton mousebindings[] = {
    [ARROW_SET] = MOUSE_BUTTON_LEFT,
    [ARROW_INTERACT] = MOUSE_BUTTON_RIGHT,
    [MOVE] = MOUSE_BUTTON_MIDDLE,
};

typedef struct Settings {
    bool black_theme;
    int number_of_threads;
    float tps;
    Vector2 camera, last_mouse_position;
    float zoom;
    bool pause;
} settings_t;

void handle_input(settings_t *settings) {
    if (IsKeyPressed(keybindings[ARROW_NORTH])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_SOUTH])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_WEST])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_EAST])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_FLIP])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_REMOVE])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[ARROW_MENU])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[SELECT])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[SELECTED_REMOVE])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[SELECTED_COPY])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[CLIPBOARD_PASTE])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR1])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR2])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR3])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR4])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR5])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR_DESELECT])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR_PREVIOUS])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[HOTBAR_NEXT])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[PIPETTE])) {
        printf("key pressed\n");
    }
    if (IsKeyDown(keybindings[MOVE_FORWARD])) {
        settings->camera.y += CAMERA_SPPED;
    }
    if (IsKeyDown(keybindings[MOVE_BACK])) {
        settings->camera.y -= CAMERA_SPPED;
    }
    if (IsKeyDown(keybindings[MOVE_LEFT])) {
        settings->camera.x += CAMERA_SPPED;
    }
    if (IsKeyDown(keybindings[MOVE_RIGHT])) {
        settings->camera.x -= CAMERA_SPPED;
    }
    if (IsKeyPressed(keybindings[CENTER])) {
        // TODO: unhardcode position
        settings->camera = (Vector2){ 0, 0 };
    }
    if (IsKeyDown(keybindings[ZOOM_IN])) {
        // TODO: unhardcode zoom
        settings->zoom *= 1.01;

        // TODO: zoom in center
        settings->camera.x -= settings->zoom/2;
        settings->camera.y -= settings->zoom/2;
    }
    if (IsKeyDown(keybindings[ZOOM_OUT])) {
        // TODO: unhardcode zoom
        settings->zoom *= 0.99;

        // TODO: zoom in center
        settings->camera.x += settings->zoom/2;
        settings->camera.y += settings->zoom/2;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(keybindings[ZOOM_RESET])) {
        settings->zoom = DEFAUL_ZOOM;
    }
    if (IsKeyPressed(keybindings[SIGNALS_REMOVE])) {
        printf("key pressed\n");
    }
    if (IsKeyPressed(keybindings[PAUSE])) {
        settings->pause = 1 - settings->pause;
    }
    if (IsKeyPressed(keybindings[MAP_MENU])) {
        printf("key pressed\n");
    }

    if (IsMouseButtonDown(mousebindings[MOVE])) {
        Vector2 mouse_position = GetMousePosition();
        Vector2 mouse_bias = {
            mouse_position.x - settings->last_mouse_position.x,
            mouse_position.y - settings->last_mouse_position.y
        };

        settings->camera.x += mouse_bias.x / settings->zoom;
        settings->camera.y += mouse_bias.y / settings->zoom;
    }

    settings->last_mouse_position = GetMousePosition();
}

int main(int argc, char** argv) {
    map_t map = { 0 };
    map_init(&map);
    map_import(&map, "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");


    struct Settings  settings;
    settings.black_theme = false;
    settings.number_of_threads = omp_get_max_threads();
    settings.tps = 60.;
    settings.zoom = DEFAUL_ZOOM;
    const int MAX_NUMBER_OF_THREADS = omp_get_max_threads();

    InitWindow(1900, 1000, "Arrows");

    Image atlas_img = LoadImage("atlas_dark.png");
    Texture atlas_dark = LoadTextureFromImage(atlas_img);
    UnloadImage(atlas_img);

    atlas_img = LoadImage("atlas.png");
    Texture atlas = LoadTextureFromImage(atlas_img);

    SetTargetFPS(60);
    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(0, BACKGROUND_COLOR)));
        for(size_t i = 0; i < hmlen(map.chunks); i++) {
            int chunk_x = map.chunks[i].key.x;
            int chunk_y = map.chunks[i].key.y;
            chunk_t *chunk = &map.chunks[i].value;
            for(size_t j = 0; j < CHUNK_SIZE*CHUNK_SIZE; j++) {
                arrow_t *arrow = &chunk->arrows[j];
                if(arrow->type == Empty) continue;
                int arrow_x = j % CHUNK_SIZE;
                int arrow_y = j / CHUNK_SIZE;
                arrow_x += chunk_x*CHUNK_SIZE;
                arrow_y += chunk_y*CHUNK_SIZE;

                Rectangle dest_rect = (Rectangle) {
                    .width = settings.zoom,
                    .height = settings.zoom,
                    .x = (arrow_x*settings.zoom+settings.camera.x+GetScreenWidth()/2.),
                    .y = (arrow_y*settings.zoom+settings.camera.y+GetScreenHeight()/2.)
                };
                Rectangle source_rect = (Rectangle) {
                    .x = ((arrow->type-1) % 8) * 256.0,
                    .y = (int)((arrow->type-1) / 8) * 256.0,
                    .width = arrow->flipped ? -256 : 256,
                    .height = 256,
                };
                if (arrow->signal != S_NONE) {
                    DrawRectangle(dest_rect.x - settings.zoom/2, dest_rect.y - settings.zoom/2, settings.zoom, settings.zoom, ({
                        Color color = settings.black_theme ? DARK_RED : RED;
                        switch (arrow->signal) {
                        case S_NONE:
                        case S_DELAY_AFTER_RED:
                        case S_BLOCK:
                            color = (Color){ 0, 0, 0, 0 };
                            break;
                        case S_RED:
                            // color = RED;
                            break;
                        case S_BLUE:
                            color = settings.black_theme ? DARK_BLUE : BLUE;
                            break;
                        case S_ORANGE:
                            color = settings.black_theme ? DARK_ORANGE : ORANGE;
                            break;
                        case S_YELLOW:
                            color = settings.black_theme ? DARK_YELLOW : YELLOW;
                            break;
                        }
                        color;
                    }));
                }
                DrawTexturePro(settings.black_theme ? atlas_dark : atlas, source_rect, dest_rect, (Vector2){ settings.zoom/2, settings.zoom/2 }, ({
                    int direction = 0;
                    switch (arrow->direction) {
                    case D_NORTH:
                        // direction = 0;
                        break;
                    case D_EAST:
                        direction = 90;
                        break;
                    case D_SOUTH:
                        direction = 180;
                        break;
                    case D_WEST:
                        direction = 270;
                        break;
                    }
                    direction;
                }), WHITE);
            }
        }
        for(size_t j = 0; j < settings.tps/60; j++) {
            map_update(&map);
        }
        // ---- GUI ----
        // TODO: unhardcode sizes
        DrawRectangle(0, 0, 100, 50, ColorAlpha(GetColor(GuiGetStyle(0, BACKGROUND_COLOR)), UI_BACKGROUND_ALPHA));
        DrawFPS(10, 10);
        DrawText(TextFormat("TPS: %f", GetFPS()*settings.tps/60.), 10, 40, 10, BLACK);
        DrawText(TextFormat("X: %f | Y: %f", settings.camera.x, settings.camera.y), 10, 60, 10, BLACK);
        int theme_button = GuiButton((Rectangle){ .x = GetScreenWidth()-100, .y = 0, .width = 100, .height = 50 }, GuiIconText(settings.black_theme ? ICON_MOON : ICON_SUN, "Theme"));
        if (theme_button) {
            settings.black_theme = 1 - settings.black_theme;
            if (settings.black_theme) {
                GuiLoadStyleDark();
            } else {
                GuiLoadStyleDefault();
            }
        }
        DrawRectangle(GetScreenWidth(), GetScreenHeight(), -250, -40, ColorAlpha(GetColor(GuiGetStyle(0, BACKGROUND_COLOR)), UI_BACKGROUND_ALPHA));
        float slider = settings.number_of_threads;
        GuiSlider((Rectangle){GetScreenWidth()-100, GetScreenHeight()-20, 100, 20}, GuiIconText(ICON_CPU, TextFormat("Number of threads: %d", settings.number_of_threads)), "", &slider, 1, MAX_NUMBER_OF_THREADS);
        if ((int)slider != settings.number_of_threads) {
            omp_set_num_threads((int)slider);
            settings.number_of_threads = (int)slider;
            slider = (float)settings.number_of_threads;
        }
        GuiSlider((Rectangle){GetScreenWidth()-100, GetScreenHeight()-40, 100, 20}, GuiIconText(ICON_CLOCK, TextFormat("TPS: %.3f", settings.tps)), "", &settings.tps, 1, MAX_TPS);
        /* if (GetFPS() < 10) { */
        /*     settings.tps /= 2.; */
        /* } */
        // ---- GUI ----
        handle_input(&settings);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
