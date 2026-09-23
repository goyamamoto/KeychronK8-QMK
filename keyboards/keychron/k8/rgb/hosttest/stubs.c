// Keyboard report model for the US-JIS tests: real mods, weak mods, up to 6 keys.
#include "quantum.h"

static uint8_t    real_mods, weak_mods;
static uint8_t    keys_down[6];
report_snapshot_t report_log[REPORT_LOG_MAX];
uint32_t          report_log_count = 0;

uint8_t get_mods(void) { return real_mods; }
void    set_mods(uint8_t m) { real_mods = m; }
void    add_mods(uint8_t m) { real_mods |= m; }
void    del_mods(uint8_t m) { real_mods &= ~m; }
uint8_t get_weak_mods(void) { return weak_mods; }
void    add_weak_mods(uint8_t m) { weak_mods |= m; }
void    del_weak_mods(uint8_t m) { weak_mods &= ~m; }
void send_keyboard_report(void) {
    if (report_log_count >= REPORT_LOG_MAX) return;
    report_snapshot_t *r = &report_log[report_log_count++];
    r->mods = real_mods | weak_mods;
    memcpy(r->keys, keys_down, sizeof(keys_down));
}
void register_code(uint8_t code) {
    for (int i = 0; i < 6; i++) if (keys_down[i] == code) { send_keyboard_report(); return; }
    for (int i = 0; i < 6; i++) if (!keys_down[i]) { keys_down[i] = code; break; }
    send_keyboard_report();
}
void unregister_code(uint8_t code) {
    for (int i = 0; i < 6; i++) if (keys_down[i] == code) keys_down[i] = 0;
    send_keyboard_report();
}
void report_model_reset(void) { real_mods = weak_mods = 0; memset(keys_down, 0, sizeof(keys_down)); report_log_count = 0; }
