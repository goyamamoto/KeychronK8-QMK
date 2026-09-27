// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>

typedef enum {
    K8_SLEEP_OK,
    K8_SLEEP_BLOCKED_BT_TX, // packet queued for the Bluetooth module
    K8_SLEEP_BLOCKED_BT_RX, // the module is sending
    K8_SLEEP_BLOCKED_KEY,   // a key is held down
    K8_SLEEP_BLOCKED_USB,   // a USB host is sending frames
} k8_sleep_blocker_t;

// Why the MCU may not sleep now, or K8_SLEEP_OK.
k8_sleep_blocker_t k8_sleep_blocker(void);

// Deep sleep until a key is pressed.
void k8_sleep(void);
