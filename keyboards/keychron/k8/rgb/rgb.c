// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

// libc rand() keeps its state in newlib's reentrancy block, which pulls in
// the stdio stream table: about 400 bytes of the 8 KiB RAM for a random
// number. This replaces it for send_string and the RGB effects.
static uint32_t k8_rand_state = 1;

void srand(unsigned int seed) {
    k8_rand_state = seed ? seed : 1;
}

int rand(void) {
    // xorshift32
    k8_rand_state ^= k8_rand_state << 13;
    k8_rand_state ^= k8_rand_state >> 17;
    k8_rand_state ^= k8_rand_state << 5;
    return (int)(k8_rand_state & RAND_MAX);
}

#ifdef BLUETOOTH_ENABLE
#    include "connection.h"
#    include "iton_bt.h"
#    ifdef USJIS_ENABLE
#        include "usjis.h"
#    endif

// Indices into DIP_SWITCH_PINS. Active means Mac and Cable, matching the
// SonixQMK K8 keymaps and the Keychron K6 v2 Bluetooth port.
#    define DIP_OS 0
#    define DIP_CONNECTION 1

// Holding Fn+1/2/3 this long puts that profile into pairing.
#    ifndef K8_BT_PAIR_HOLD_TIME
#        define K8_BT_PAIR_HOLD_TIME 3000
#    endif

// Backlight goes dark after this much idle time on Bluetooth to save battery.
#    ifndef K8_BT_RGB_TIMEOUT
#        define K8_BT_RGB_TIMEOUT 300000
#    endif

// Give the module time to boot before the first command.
#    define K8_BT_STARTUP_DELAY 500
#    define K8_BT_OS_DELAY 300
#    define K8_BT_EVENT_TIME 3000

#    define K8_BT_PROFILES 3

typedef union {
    uint32_t raw;
    struct {
        uint8_t profile;
        bool    usjis : 1;
    };
} k8_config_t;

typedef enum {
    BT_IDLE,
    BT_CONNECTING,
    BT_PAIRING,
    BT_CONNECTED,
    BT_DISCONNECTED,
} bt_state_t;

static k8_config_t k8_config;

static bool     bt_mode;
static bool     mac_mode;
static bool     module_needs_mode = true;
static uint16_t startup_timer;

static bool     pair_hold_active;
static uint16_t pair_hold_timer;

static bool rgb_idle_off;

#    ifdef USJIS_ENABLE
static bool     usjis_show;
static uint16_t usjis_show_timer;
#    endif

// Written from the module's IRQ callbacks.
static volatile bt_state_t bt_state;
static volatile uint16_t   bt_state_timer;
static volatile uint8_t    battery_level;
static volatile uint16_t   battery_timer;
static volatile bool       battery_low;

static void set_bt_state(bt_state_t state) {
    bt_state       = state;
    bt_state_timer = timer_read();
}

void iton_bt_enters_connection_state(void) {
    set_bt_state(BT_CONNECTING);
}

void iton_bt_entered_pairing(void) {
    set_bt_state(BT_PAIRING);
}

void iton_bt_connection_successful(void) {
    set_bt_state(BT_CONNECTED);
}

void iton_bt_disconnected(void) {
    set_bt_state(BT_DISCONNECTED);
}

void iton_bt_battery_level(uint8_t level) {
    battery_level = level;
    battery_timer = timer_read();
}

void iton_bt_battery_voltage_low(void) {
    battery_low = true;
}

void iton_bt_battery_exit_low_battery_mode(void) {
    battery_low = false;
}

static bool     os_pending;
static uint16_t os_timer;

static void send_mode_to_module(void) {
    if (bt_mode) {
        iton_bt_switch_profile(k8_config.profile);
        // The module ignores packets for a while after a profile switch.
        os_pending = true;
        os_timer   = timer_read();
    } else {
        iton_bt_mode_usb();
    }
}

static void send_os_to_module(void) {
    if (mac_mode) {
        iton_bt_os_mac();
    } else {
        iton_bt_os_win();
    }
}

static void select_profile(uint8_t profile) {
    if (k8_config.profile != profile) {
        k8_config.profile = profile;
        eeconfig_update_kb(k8_config.raw);
    }
    iton_bt_switch_profile(profile);
}

#    ifdef USJIS_ENABLE
bool usjis_host_is_win(void) {
    return !mac_mode;
}

void usjis_mode_applied(bool enabled) {
    k8_config.usjis = enabled;
    eeconfig_update_kb(k8_config.raw);
    usjis_show       = true;
    usjis_show_timer = timer_read();
}
#    endif

void eeconfig_init_kb(void) {
    k8_config.raw = 0;
    eeconfig_update_kb(k8_config.raw);
    eeconfig_init_user();
}

void keyboard_post_init_kb(void) {
    k8_config.raw = eeconfig_read_kb();
    if (k8_config.profile >= K8_BT_PROFILES) {
        k8_config.profile = 0;
    }
#    ifdef USJIS_ENABLE
    usjis_init(k8_config.usjis);
#    endif
    startup_timer = timer_read();
    keyboard_post_init_user();
}

bool dip_switch_update_kb(uint8_t index, bool active) {
    switch (index) {
        case DIP_OS:
            mac_mode          = active;
            module_needs_mode = true;
            break;
        case DIP_CONNECTION:
            bt_mode = !active;
            connection_set_host_noeeprom(bt_mode ? CONNECTION_HOST_BLUETOOTH : CONNECTION_HOST_USB);
            module_needs_mode = true;
            pair_hold_active  = false;
            set_bt_state(BT_IDLE);
            break;
    }
    return dip_switch_update_user(index, active);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
#    ifdef USJIS_ENABLE
    if (!usjis_process_record(keycode, record)) {
        return false;
    }
#    endif

    switch (keycode) {
#    ifdef USJIS_ENABLE
        case USJIS_TOGGLE:
            if (record->event.pressed) {
                usjis_toggle();
            }
            return false;
        case USJIS_ON:
        case USJIS_OFF:
            if (record->event.pressed) {
                usjis_request(keycode == USJIS_ON);
            }
            return false;
#    endif
        case BT_PRF1 ... BT_PRF3:
            if (!bt_mode) {
                return false;
            }
            if (record->event.pressed) {
                select_profile(keycode - BT_PRF1);
                pair_hold_active = true;
                pair_hold_timer  = timer_read();
            } else {
                pair_hold_active = false;
            }
            return false;
        case K8_BATT:
            if (bt_mode && record->event.pressed) {
                iton_bt_query_battery_level();
            }
            return false;
    }
    return true;
}

#    ifdef USJIS_ENABLE
void post_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    usjis_post_process_record(keycode, record);
    post_process_record_user(keycode, record);
}
#    endif

static void rgb_idle_task(void) {
    bool idle = bt_mode && last_input_activity_elapsed() > K8_BT_RGB_TIMEOUT;

    if (idle && !rgb_idle_off && rgb_matrix_is_enabled()) {
        rgb_matrix_disable_noeeprom();
        rgb_idle_off = true;
    } else if (!idle && rgb_idle_off) {
        rgb_matrix_enable_noeeprom();
        rgb_idle_off = false;
    }
}

#    ifdef ITON_BT_DEBUG
#        include "print.h"

// Print link events as they arrive, plus switch state changes.
static void debug_task(void) {
    static uint8_t printed;
    static int8_t  last_bt = -1;

    // Give the host time to reattach the console after a reset.
    if (timer_read32() < 3000) {
        return;
    }

    if (last_bt != bt_mode) {
        last_bt = bt_mode;
        uprintf("%5u dip bt=%d mac=%d\n", timer_read(), bt_mode, mac_mode);
    }
    while (printed != iton_bt_log_head) {
        if ((uint8_t)(iton_bt_log_head - printed) > ITON_BT_LOG_LEN) {
            uprintf("... %u events lost\n", (uint8_t)(iton_bt_log_head - printed) - ITON_BT_LOG_LEN);
            printed = iton_bt_log_head - ITON_BT_LOG_LEN;
        }
        volatile iton_bt_event_t *e = &iton_bt_log[printed % ITON_BT_LOG_LEN];
        uprintf("%5u %c %02X %02X %02X\n", e->time, e->type, e->data[0], e->data[1], e->data[2]);
        printed++;
    }
}
#    endif

void housekeeping_task_kb(void) {
    if (module_needs_mode && timer_elapsed(startup_timer) > K8_BT_STARTUP_DELAY) {
        module_needs_mode = false;
        send_mode_to_module();
    }

    if (os_pending && timer_elapsed(os_timer) > K8_BT_OS_DELAY) {
        os_pending = false;
        send_os_to_module();
    }

    if (pair_hold_active && timer_elapsed(pair_hold_timer) > K8_BT_PAIR_HOLD_TIME) {
        pair_hold_active = false;
        iton_bt_enter_pairing();
    }

    rgb_idle_task();
#    ifdef ITON_BT_DEBUG
    debug_task();
#    endif
    housekeeping_task_user();
}

static void set_color_in_range(uint8_t index, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {
    if (index != NO_LED && index >= led_min && index < led_max) {
        rgb_matrix_set_color(index, r, g, b);
    }
}

static bool blink(uint16_t period) {
    return (timer_read() / period) % 2 == 0;
}

// The number row keys "1" to "0" sit at matrix row 1, columns 1 to 10.
static uint8_t number_key_led(uint8_t n) {
    return g_led_config.matrix_co[1][1 + n];
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

#    ifdef USJIS_ENABLE
    // US-JIS mode change: Tab blinks green (on) or red (off).
    if (usjis_show) {
        if (timer_elapsed(usjis_show_timer) >= K8_BT_EVENT_TIME) {
            usjis_show = false;
        } else if (blink(250)) {
            if (usjis_is_enabled()) {
                set_color_in_range(g_led_config.matrix_co[2][0], led_min, led_max, RGB_GREEN);
            } else {
                set_color_in_range(g_led_config.matrix_co[2][0], led_min, led_max, RGB_RED);
            }
        } else {
            set_color_in_range(g_led_config.matrix_co[2][0], led_min, led_max, RGB_OFF);
        }
    }
#    endif

    if (!bt_mode) {
        return true;
    }

    uint8_t profile_led = number_key_led(k8_config.profile);
    switch (bt_state) {
        // Blink against black so the key stands out from any animation.
        case BT_PAIRING:
            if (blink(250)) {
                set_color_in_range(profile_led, led_min, led_max, RGB_BLUE);
            } else {
                set_color_in_range(profile_led, led_min, led_max, RGB_OFF);
            }
            break;
        case BT_CONNECTING:
            if (blink(500)) {
                set_color_in_range(profile_led, led_min, led_max, RGB_WHITE);
            } else {
                set_color_in_range(profile_led, led_min, led_max, RGB_OFF);
            }
            break;
        case BT_CONNECTED:
            if (timer_elapsed(bt_state_timer) < K8_BT_EVENT_TIME) {
                set_color_in_range(profile_led, led_min, led_max, RGB_GREEN);
            }
            break;
        case BT_DISCONNECTED:
            if (timer_elapsed(bt_state_timer) < K8_BT_EVENT_TIME) {
                set_color_in_range(profile_led, led_min, led_max, RGB_RED);
            }
            break;
        case BT_IDLE:
            break;
    }

    if (battery_level && timer_elapsed(battery_timer) < K8_BT_EVENT_TIME) {
        uint8_t keys = battery_level == batt_above_70 ? 10 : battery_level == batt_between_30_70 ? 6 : 3;
        for (uint8_t i = 0; i < keys; i++) {
            switch (battery_level) {
                case batt_above_70:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_GREEN);
                    break;
                case batt_between_30_70:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_YELLOW);
                    break;
                default:
                    set_color_in_range(number_key_led(i), led_min, led_max, RGB_RED);
                    break;
            }
        }
    }

    if (battery_low && blink(1000)) {
        set_color_in_range(g_led_config.matrix_co[0][0], led_min, led_max, RGB_RED);
    }
    return true;
}
#endif
