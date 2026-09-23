#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint8_t row; uint8_t col; } keypos_t;
typedef struct { keypos_t key; bool pressed; uint16_t time; } keyevent_t;
typedef struct { keyevent_t event; } keyrecord_t;
