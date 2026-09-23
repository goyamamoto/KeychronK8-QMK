// Copyright 2021 1Conan (@1Conan)
// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

#ifdef BLUETOOTH_ITON_BT
#    undef SN32_SPI_USE_SPI0
#    define SN32_SPI_USE_SPI0 TRUE
#endif
