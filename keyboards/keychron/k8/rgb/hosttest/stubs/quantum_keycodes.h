// Host-test stand-in for QMK's quantum_keycodes.h: the modifier-keycode
// helpers the code under test uses. keycodes.h and modifiers.h are QMK's own.
#pragma once
#include "keycodes.h"
#include "modifiers.h"
#define QK_MODS 0x0100
#define QK_MODS_MAX 0x1FFF
#define QK_MODS_GET_MODS(kc) (((kc) >> 8) & 0x1F)
#define QK_MODS_GET_BASIC_KEYCODE(kc) ((kc) & 0xFF)
#define IS_QK_MODS(code) ((code) >= QK_MODS && (code) <= QK_MODS_MAX)
#define QK_LSFT 0x0200
#define LSFT(kc) (QK_LSFT | (kc))
#define QK_KB_0 0x7E00
#ifndef MOD_BIT
#    define MOD_BIT(code) (1 << ((code) & 0x07))
#endif
#ifndef MOD_LSFT
#    define MOD_LSFT 0x02
#    define MOD_RSFT 0x20
#endif
#ifndef MOD_MASK_SHIFT
#    define MOD_MASK_SHIFT 0x22
#endif
