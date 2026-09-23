// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// VIA keeps four layers of 6x17 keycodes in EEPROM: 816 bytes of the 1 KiB.
#undef WEAR_LEVELING_LOGICAL_SIZE
#define WEAR_LEVELING_LOGICAL_SIZE 1024
