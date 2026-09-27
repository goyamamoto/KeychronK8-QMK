// Copyright 2021 1Conan (@1Conan)
// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef BLUETOOTH_ITON_BT
// SPI0 runs at the highest IRQ priority; the Keychron K6 v2 port saw erratic
// behaviour with the default. The direction line (A1) does not use PAL
// callbacks: ITON_BT_SN32_LINE_IRQ in config.h handles it at priority 0
// without their 512-byte event table.

#    define HAL_USE_SPI TRUE
#    define SPI_USE_MUTUAL_EXCLUSION FALSE
#    define SPI_USE_WAIT FALSE
#    define SPI_USE_ASSERT_ON_ERROR FALSE
#    define SPI_SELECT_MODE SPI_SELECT_MODE_NONE
#    define SN32_SPI_SPI0_IRQ_PRIORITY 0
#endif

#include_next <halconf.h>
