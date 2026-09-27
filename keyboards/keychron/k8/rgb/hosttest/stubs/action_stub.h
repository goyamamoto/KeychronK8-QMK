#pragma once
// Report model for the US-JIS tests: real mods, weak mods, up to 6 keys.
#include <stdint.h>
#include <stdbool.h>
uint8_t get_mods(void);
void    set_mods(uint8_t mods);
void    add_mods(uint8_t mods);
void    del_mods(uint8_t mods);
uint8_t get_weak_mods(void);
void    add_weak_mods(uint8_t mods);
void    del_weak_mods(uint8_t mods);
void    register_code(uint8_t code);
void    unregister_code(uint8_t code);
void    send_keyboard_report(void);

#define REPORT_LOG_MAX 256
typedef struct { uint8_t mods; uint8_t keys[6]; } report_snapshot_t;
extern report_snapshot_t report_log[REPORT_LOG_MAX];
extern uint32_t          report_log_count;
void                     report_model_reset(void);
