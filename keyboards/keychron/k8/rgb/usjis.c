// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later
//
// US-JIS substitution.
//
// A host set to the Japanese keyboard layout reads the US keys ` ~ @ ^ & *
// ( ) _ = + [ { ] } \ | : ' " differently from their printing. When the mode
// is enabled and the host is treated as Windows, the twenty chords below are
// replaced by the JIS chord that produces the printed character. Everything
// else is left alone.
//
// Rules (from the zmk-kb1-usjis specification):
//  - The decision is made once, when the key goes down, from the key and
//    the Shift state at that moment (physical Shift or Shift carried by the
//    keycode). Ctrl, Alt and GUI are ignored for the decision and kept.
//  - A substituted key stays held on the host until the physical key is
//    released; the release uses what was decided at press.
//  - While at least one substituted key is held, the report's Shift follows
//    the most recently pressed non-modifier key: a substituted key asks for
//    Shift or no Shift, an unsubstituted key gets the physical Shift keys.
//    Releasing the newest key returns to the previous key's request; a
//    physical Shift change re-applies the same policy.
//  - The Shift for a key event is set before QMK sends the report that
//    carries the event, so the host sees the key and its Shift together: the
//    host picks the character from the report where the key goes down.
//  - Once no substituted key is held, Shift follows the physical keys again.
//  - A mode change while any non-modifier key is held is postponed until
//    the last one is released, so one key never sees both modes.

#include "usjis.h"
#include "quantum.h"

#define USJIS_MAX_HELD 12

typedef struct {
    uint16_t keycode;     // keycode as seen by process_record (identifies the key)
    uint8_t  out;         // basic keycode reported to the host
    bool     substituted; // out differs from the key, we own its report
    bool     shift;       // Shift this key wants in the report
} usjis_held_t;

typedef struct {
    uint8_t in;    // US basic keycode
    bool    shift; // with Shift
    uint8_t out;   // JIS basic keycode
    bool    oshift;
} usjis_rule_t;

// C01..C20. JIS positions: ^ = KC_EQL, @ = KC_LBRC, [ = KC_RBRC, ] = KC_NUHS,
// : = KC_QUOT, backslash/underscore = KC_INT1, yen = KC_INT3.
static const usjis_rule_t rules[] = {
    {KC_GRV, true, KC_EQL, true},     // C01 ~
    {KC_2, true, KC_LBRC, false},     // C02 @
    {KC_6, true, KC_EQL, false},      // C03 ^
    {KC_7, true, KC_6, true},         // C04 &
    {KC_8, true, KC_QUOT, true},      // C05 *
    {KC_9, true, KC_8, true},         // C06 (
    {KC_0, true, KC_9, true},         // C07 )
    {KC_MINS, true, KC_INT1, true},   // C08 _
    {KC_EQL, false, KC_MINS, true},   // C09 =
    {KC_EQL, true, KC_SCLN, true},    // C10 +
    {KC_LBRC, false, KC_RBRC, false}, // C11 [
    {KC_LBRC, true, KC_RBRC, true},   // C12 {
    {KC_RBRC, false, KC_NUHS, false}, // C13 ]
    {KC_RBRC, true, KC_NUHS, true},   // C14 }
    {KC_BSLS, false, KC_INT1, false}, // C15 backslash
    {KC_BSLS, true, KC_INT3, true},   // C16 |
    {KC_SCLN, true, KC_QUOT, false},  // C17 :
    {KC_QUOT, false, KC_7, true},     // C18 '
    {KC_QUOT, true, KC_2, true},      // C19 "
    {KC_GRV, false, KC_LBRC, true},   // C20 `
};

static usjis_held_t held[USJIS_MAX_HELD];
static uint8_t      held_count   = 0;
static uint8_t      phys_shift   = 0;  // MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT) as pressed
static int8_t       pending_mode = -1; // -1 none, 0 disable, 1 enable
static bool         enabled      = false;

bool usjis_is_enabled(void) {
    return enabled;
}

static bool usjis_is_active(void) {
    return enabled && usjis_host_is_win();
}

void usjis_init(bool enable) {
    enabled      = enable;
    pending_mode = -1;
}

static void apply_mode(bool enable) {
    if (enabled == enable) return;
    enabled = enable;
    usjis_mode_applied(enable);
}

void usjis_request(bool enable) {
    bool base = pending_mode >= 0 ? pending_mode : usjis_is_enabled();
    if (base == enable) return;
    if (held_count) {
        pending_mode = enable;
    } else {
        apply_mode(enable);
    }
}

void usjis_toggle(void) {
    bool base = pending_mode >= 0 ? pending_mode : usjis_is_enabled();
    usjis_request(!base);
}

static bool any_substituted_held(void) {
    for (uint8_t i = 0; i < held_count; i++) {
        if (held[i].substituted) return true;
    }
    return false;
}

// The report's modifiers under the policy: the newest held key decides Shift.
// A substituted key asks for the Shift of its JIS chord; any other key, or no
// key, gets the physical Shift keys as they are.
static uint8_t policy_mods(void) {
    uint8_t mods = get_mods() & (uint8_t)~MOD_MASK_SHIFT;
    if (held_count && held[held_count - 1].substituted) {
        if (held[held_count - 1].shift) mods |= MOD_BIT(KC_LSFT);
    } else {
        mods |= phys_shift;
    }
    return mods;
}

// Put the policy into the modifier state without sending anything, so that
// the next report, the one carrying the key event, already has it.
static void set_policy_mods(void) {
    set_mods(policy_mods());
    if (held_count && held[held_count - 1].substituted) del_weak_mods(MOD_MASK_SHIFT);
}

static int8_t find_held(uint16_t keycode) {
    for (int8_t i = held_count - 1; i >= 0; i--) {
        if (held[i].keycode == keycode) return i;
    }
    return -1;
}

static void remove_held(uint8_t index) {
    for (uint8_t i = index; i + 1 < held_count; i++) {
        held[i] = held[i + 1];
    }
    held_count--;
}

static const usjis_rule_t *lookup(uint8_t kc, bool shift) {
    for (uint8_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        if (rules[i].in == kc && rules[i].shift == shift) return &rules[i];
    }
    return NULL;
}

static void apply_pending_if_idle(void) {
    if (held_count == 0 && pending_mode >= 0) {
        bool enable  = pending_mode;
        pending_mode = -1;
        apply_mode(enable);
    }
}

void usjis_clear(void) {
    held_count = 0;
    phys_shift = get_mods() & MOD_MASK_SHIFT;
    apply_pending_if_idle();
}

static bool is_shift_key(uint16_t keycode) {
    return keycode == KC_LSFT || keycode == KC_RSFT;
}

static bool is_modifier_key(uint16_t keycode) {
    return keycode >= KC_LCTL && keycode <= KC_RGUI;
}

// A key we track: a basic keycode, possibly with modifiers folded in (LSFT(KC_2)).
static bool is_plain_key(uint16_t keycode) {
    if (IS_QK_MODS(keycode)) keycode = QK_MODS_GET_BASIC_KEYCODE(keycode);
    return keycode >= KC_A && keycode <= KC_EXSEL && !is_modifier_key(keycode);
}

bool usjis_process_record(uint16_t keycode, keyrecord_t *record) {
    if (is_shift_key(keycode)) {
        uint8_t bit = keycode == KC_LSFT ? MOD_BIT(KC_LSFT) : MOD_BIT(KC_RSFT);
        if (record->event.pressed) {
            phys_shift |= bit;
        } else {
            phys_shift &= ~bit;
        }
        if (!any_substituted_held()) return true; // plain Shift, QMK reports it
        set_policy_mods();
        send_keyboard_report(); // one report per physical Shift event
        return false;
    }

    if (!is_plain_key(keycode)) return true;

    if (record->event.pressed) {
        if (held_count >= USJIS_MAX_HELD) return true;

        uint8_t kc          = QK_MODS_GET_BASIC_KEYCODE(keycode);
        bool    keycode_sft = IS_QK_MODS(keycode) && (QK_MODS_GET_MODS(keycode) & (MOD_LSFT | MOD_RSFT));
        bool    eff_shift   = keycode_sft || phys_shift;
        const usjis_rule_t *rule = usjis_is_active() ? lookup(kc, eff_shift) : NULL;

        usjis_held_t *h = &held[held_count++];
        h->keycode      = keycode;
        h->substituted  = rule != NULL;
        h->out          = rule ? rule->out : kc;
        h->shift        = rule ? rule->oshift : eff_shift;

        if (!rule) {
            // QMK reports the key down; give that report this key's Shift.
            if (any_substituted_held()) set_policy_mods();
            return true;
        }

        set_policy_mods();
        register_code(h->out);
        return false;
    }

    int8_t i = find_held(keycode);
    if (i < 0) return true;

    bool substituted = held[i].substituted;
    uint8_t out      = held[i].out;
    remove_held(i);

    if (!substituted) {
        // QMK reports the key up; give that report the remaining keys' Shift.
        if (any_substituted_held()) set_policy_mods();
        return true;
    }

    set_policy_mods();
    unregister_code(out);
    apply_pending_if_idle();
    return false;
}

void usjis_post_process_record(uint16_t keycode, keyrecord_t *record) {
    // Other processing (a mod-tap resolving, for example) may have touched the
    // modifiers while a substituted key is held; put the policy back.
    if (any_substituted_held() && get_mods() != policy_mods()) {
        set_policy_mods();
        send_keyboard_report();
    }
    if (!record->event.pressed) apply_pending_if_idle();
}
