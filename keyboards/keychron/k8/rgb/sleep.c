// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

// Deep sleep on Bluetooth: the key columns are driven high and a rising edge
// on any key row wakes the MCU. The Bluetooth module stays on and keeps the
// connection.

#include QMK_KEYBOARD_H
#include "sleep.h"
#include "sn32f2xx.h"

// LED rows low and key columns high turn every LED off.
#if SN32F2XX_RGB_OUTPUT_ACTIVE_LEVEL != SN32F2XX_RGB_OUTPUT_ACTIVE_HIGH || SN32F2XX_PWM_OUTPUT_ACTIVE_LEVEL != SN32F2XX_PWM_OUTPUT_ACTIVE_LOW
#    error "sleep.c assumes RGB rows active high and PWM columns active low"
#endif

static const pin_t row_pins[MATRIX_ROWS]                  = MATRIX_ROW_PINS;
static const pin_t col_pins[MATRIX_COLS]                  = MATRIX_COL_PINS;
static const pin_t led_row_pins[SN32F2XX_RGB_MATRIX_ROWS_HW] = SN32F2XX_RGB_MATRIX_ROW_PINS;

void matrix_init_pins(void);
void last_matrix_activity_trigger(void); // quantum/keyboard.c

// As in iton_bt.c.
#ifndef ITON_BT_IRQ_LINE
#    define ITON_BT_IRQ_LINE A0
#endif
#ifndef ITON_BT_INT_LINE
#    define ITON_BT_INT_LINE A1
#endif
#ifndef ITON_BT_LINE_IRQ_PRIORITY
#    define ITON_BT_LINE_IRQ_PRIORITY 0
#endif

static volatile bool woken_by_key;

// Both key-row ports: clear the flags and stop.
static void row_wake_irq(ioportid_t port, uint32_t irq) {
    port->IE = 0;
    port->IC = 0xFFFF;
    nvicDisableVector(irq);
    woken_by_key = true;
}

OSAL_IRQ_HANDLER(SN32_GPIOC_HANDLER) {
    OSAL_IRQ_PROLOGUE();
    row_wake_irq(GPIOC, SN32_GPIOC_NUMBER);
    OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(SN32_GPIOD_HANDLER) {
    OSAL_IRQ_PROLOGUE();
    row_wake_irq(GPIOD, SN32_GPIOD_NUMBER);
    OSAL_IRQ_EPILOGUE();
}

static void enable_row_wake(void) {
    uint32_t bits[2] = {0, 0}; // GPIOC, GPIOD

    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        gpio_set_pin_input(row_pins[i]); // no pull: the board pulls the rows down
        if (PAL_PORT(row_pins[i]) == GPIOC) {
            bits[0] |= 1U << PAL_PAD(row_pins[i]);
        } else if (PAL_PORT(row_pins[i]) == GPIOD) {
            bits[1] |= 1U << PAL_PAD(row_pins[i]);
        }
    }

    const ioportid_t ports[2] = {GPIOC, GPIOD};
    const uint32_t   irqs[2]  = {SN32_GPIOC_NUMBER, SN32_GPIOD_NUMBER};
    for (uint8_t p = 0; p < 2; p++) {
        if (!bits[p]) {
            continue;
        }
        ports[p]->IS &= ~bits[p];  // edge
        ports[p]->IBS &= ~bits[p]; // one edge
        ports[p]->IEV &= ~bits[p]; // rising
        ports[p]->IC = 0xFFFF;
        ports[p]->IE = bits[p];
        nvicClearPending(irqs[p]);
        nvicEnableVector(irqs[p], 3);
    }
}

static void disable_row_wake(void) {
    GPIOC->IE = 0;
    GPIOD->IE = 0;
    GPIOC->IC = 0xFFFF;
    GPIOD->IC = 0xFFFF;
    nvicDisableVector(SN32_GPIOC_NUMBER);
    nvicDisableVector(SN32_GPIOD_NUMBER);
}

// ILRC, AHB/4 and the slow flash timing, then IHRC off: the datasheet's
// advice before deep sleep on IHRC.
static void clock_slow(void) {
    SN_SYS0->CLKCFG = 0x1;
    while ((SN_SYS0->CLKCFG & 0x70) != 0x10) {
    }
    SN_SYS0->AHBCP  = 0x2;
    SN_FLASH->LPCTRL = 0x5AFA0002;
    SN_SYS0->ANBCTRL = 0x0;
}

// Back to IHRC 48 MHz: flash timing 4 then 5, never straight from 2 to 5.
static void clock_restore(void) {
    SN_FLASH->LPCTRL = 0x5AFA0004;
    SN_FLASH->LPCTRL = 0x5AFA0005;
    SN_SYS0->ANBCTRL = 0x1;
    while ((SN_SYS0->CSST & 0x1) != 0x1) {
    }
    SN_SYS0->CLKCFG = 0x0;
    while ((SN_SYS0->CLKCFG & 0x70) != 0x0) {
    }
    SN_SYS0->AHBCP = 0x0;
    SystemCoreClockUpdate();
}

// The K8 cannot see VBUS, so the USB driver stays "active" after the cable
// is pulled. A host sends a start-of-frame every millisecond; if the frame
// number moves, a host is there.
static bool usb_host_present(void) {
    uint32_t frame = SN_USB->FRMNO & 0x7FF;
    wait_ms(3);
    return (SN_USB->FRMNO & 0x7FF) != frame;
}

k8_sleep_blocker_t k8_sleep_blocker(void) {
    // Not while a packet is on its way to the module (A0 high until the
    // module has clocked it out) or from it (A1 high).
    if (gpio_read_pin(ITON_BT_IRQ_LINE)) {
        return K8_SLEEP_BLOCKED_BT_TX;
    }
    if (gpio_read_pin(ITON_BT_INT_LINE)) {
        return K8_SLEEP_BLOCKED_BT_RX;
    }
    // Nor with a key held down: its row is already high, so no key on that
    // row could wake the MCU.
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        if (matrix_get_row(row)) {
            return K8_SLEEP_BLOCKED_KEY;
        }
    }
    // Nor while a host is using USB.
    if (usb_host_present()) {
        return K8_SLEEP_BLOCKED_USB;
    }
    return K8_SLEEP_OK;
}

void k8_sleep(void) {
    // Stop the LED and key-scan timer and take the columns back from it.
    nvicDisableVector(SN32_CT16B1_NUMBER);
    uint32_t tmrctrl  = SN_CT16B1->TMRCTRL;
    uint32_t pwmenb   = SN_CT16B1->PWMENB;
    uint32_t pwmioenb = SN_CT16B1->PWMIOENB;
    SN_CT16B1->TMRCTRL  = 0;
    SN_CT16B1->PWMENB   = 0;
    SN_CT16B1->PWMIOENB = 0;

    for (uint8_t i = 0; i < SN32F2XX_RGB_MATRIX_ROWS_HW; i++) {
        gpio_set_pin_output(led_row_pins[i]);
        gpio_write_pin_low(led_row_pins[i]);
    }
    bool caps = gpio_read_pin(LED_CAPS_LOCK_PIN);
    gpio_write_pin_low(LED_CAPS_LOCK_PIN);

    for (uint8_t i = 0; i < MATRIX_COLS; i++) {
        gpio_set_pin_output(col_pins[i]);
        gpio_write_pin_high(col_pins[i]);
    }

    // Only keys wake the MCU, not the Bluetooth module.
    nvicDisableVector(SN32_GPIOA_NUMBER);

    woken_by_key = false;
    enable_row_wake();

    // Anything else that is pending (the system timer, USB) ends WFI at once;
    // serve it and go back to sleep until a key wakes us.
    while (!woken_by_key) {
        chSysLock();
        if (!woken_by_key) {
            clock_slow();
            SN_PMU->CTRL = 0x2; // WFI enters deep sleep
            __WFI();
            SN_PMU->CTRL = 0x0;
            clock_restore();
        }
        chSysUnlock();
    }

    disable_row_wake();
    GPIOA->IC = 1U << PAL_PAD(ITON_BT_INT_LINE);
    nvicClearPending(SN32_GPIOA_NUMBER);
    nvicEnableVector(SN32_GPIOA_NUMBER, ITON_BT_LINE_IRQ_PRIORITY);

    matrix_init_pins();
    gpio_write_pin(LED_CAPS_LOCK_PIN, caps);

    SN_CT16B1->PWMIOENB = pwmioenb;
    SN_CT16B1->PWMENB   = pwmenb;
    SN_CT16B1->TMRCTRL  = tmrctrl;
    nvicClearPending(SN32_CT16B1_NUMBER);
    nvicEnableVector(SN32_CT16B1_NUMBER, SN32_PWM_CT16B1_IRQ_PRIORITY);

    // The system timer stood still while asleep; start the idle time afresh.
    last_matrix_activity_trigger();
}
