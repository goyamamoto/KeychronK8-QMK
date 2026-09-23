// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later
//
// US-JIS substitution: type a US keyboard as printed on a host that is set
// to the Japanese keyboard layout. See usjis.c for the rules.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "action.h"

// Call first from process_record_kb. Returns false when the event was consumed.
bool usjis_process_record(uint16_t keycode, keyrecord_t *record);
// Call from post_process_record_kb: re-applies the Shift policy after QMK
// handled a pass-through event (modifier keys, unsubstituted keys).
void usjis_post_process_record(uint16_t keycode, keyrecord_t *record);
// Forget every held key, for example after the host report was cleared.
void usjis_clear(void);
// Set the effective mode read from storage at boot, without saving it.
void usjis_init(bool enabled);
// Request a mode change; applied once no non-modifier key is held.
void usjis_request(bool enable);
void usjis_toggle(void);
// Effective mode.
bool usjis_is_enabled(void);

// Provided by the keyboard.
// True when the host is treated as Windows; substitution only applies then.
bool usjis_host_is_win(void);
// Called when a mode change takes effect: save it and show it.
void usjis_mode_applied(bool enabled);
