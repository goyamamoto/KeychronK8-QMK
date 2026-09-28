# Keychron K8 under Renode

Runs the K8 firmware on an emulated SN32F248B with a model of the Bluetooth
module, and checks the Bluetooth behaviour without a keyboard: what the
firmware sends to the module, what the host gets, and how the firmware
reacts to what the module sends.

## Run

Build the emulation image (from the repository root, inside the QMK
toolchain), then run the tests:

    docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
        keyboards/keychron/k8/sim/renode/build.sh
    keyboards/keychron/k8/sim/renode/run.sh .build/keychron_k8_rgb_ansi_ansi_renode.elf

`run.sh` prints one line per check and exits 1 if any check fails. The full
log is in `out/tests.log`; `K8_SIM_VERBOSE=1` adds every packet between the
firmware and the module. Needs Renode 1.17 or newer
(https://github.com/renode/renode/releases): `renode` on PATH or
`RENODE=/path/to/renode`. About 7 minutes, most of it the sleep tests.

## What the tests cover

| Area | Checks |
|---|---|
| Start-up | profile switch, then the OS command 300 ms later (Windows, Mac); USB mode on the cable |
| Connection | bonded profile connects; disconnect shown |
| Typing | key down and up; five keys in the HID report and the sixth as a bitmap; volume key |
| Host LEDs | Caps Lock state from the module drives the Caps Lock LED |
| Battery | Fn+B query and level; low-battery flag set and cleared |
| Pairing | Fn+1 switches, holding it 3 s pairs, typing reaches the new host |
| Profiles | Fn+2 switches and is saved |
| Mode switch | Cable and back to Bluetooth, reconnecting |
| Stuck module | the firmware gives up after the send timeout and recovers |
| Sleep | deep sleep after the idle time, key wake, link kept, typing after wake; no sleep with a USB host or on the cable |
| USB suspend | Bluetooth typing goes on while USB is suspended |

Lines starting with `KNOWN` are behaviour the tests show but that is not
fixed yet; they do not fail the run. There is one: only keys wake the MCU
from deep sleep, so a Caps Lock change the host sends meanwhile is lost (if
the real module does not send it again) and the LED stays as it was.

The USB suspend check covers the cable plugged into a sleeping computer
while the switch is on Bluetooth: with Bluetooth enabled QMK builds without
the loop that waits while USB is suspended (`NO_USB_STARTUP_CHECK`).

## Not covered

USB enumeration and USB typing, how the LEDs look (the RGB effects run but
nothing checks the colours), US-JIS (see `rgb/hosttest`), the real radio and
the real module's timing, battery life, and what the models below leave out:
the clocks keep full speed in sleep, flash writes take no time.

## Emulation images

`build.sh` builds the `ansi` keymap with two changes, as
`.build/keychron_k8_rgb_ansi_ansi_renode.elf`. Never flash it.

* `CORTEX_ALTERNATE_SWITCH=TRUE`: ChibiOS switches threads through PendSV.
  The default ARMv6-M port ends interrupts with an NMI, which Renode's
  Cortex-M0 never takes, and the firmware stops at the first interrupt.
* `K8_BT_RGB_TIMEOUT` 10 s and `K8_BT_SLEEP_TIMEOUT` 20 s instead of 5 and
  10 minutes, so the sleep tests take seconds.

## Files

| File | What |
|---|---|
| `k8.repl` | the platform: Cortex-M0, 64 KB flash, 8 KB RAM, the models below, pins of `keyboards/keychron/k8/rgb` |
| `sn32.cs` | SN32F248B models: clocks (SYS0), power unit, flash controller, GPIO with the key matrix, CT16B timers, SPI0 as slave, USB (frame counter and bus suspend only) |
| `iton.cs` | the Bluetooth module and its host |
| `k8test.py`, `tests.py`, `tests.resc` | the test helpers and the tests |
| `symbols.py` | writes the firmware's symbol table for the tests (Renode's own lookup can return a neighbouring variable) |
| `build.sh`, `run.sh` | build the images, run the tests |

## The Bluetooth module model

The firmware side is the real code (`drivers/bluetooth/iton_bt.c`,
`keyboards/keychron/k8/rgb/rgb.c`, `sleep.c`). The module side is a model:
packet codes from `iton_bt.h`, reactions and timings chosen to match what
the K8 does on the desk, not measured:

* it starts clocking a packet out of the MCU 100 µs after A0 rises, one byte
  every 20 µs, until A0 falls. Renode runs the CPU in time slices, so the
  model waits for the MCU's SPI interrupt to queue the next byte, as the real
  interrupt does within microseconds;
* after a profile switch it ignores packets for 200 ms (on the K8 the OS
  command was lost when sent right after the switch);
* a bonded profile connects 800 ms after the switch; `HostPair` bonds a
  profile in pairing;
* packets to the MCU wait until no packet from the MCU is in flight, and are
  lost while the MCU is in deep sleep (the SPI block has no clock then).

Change these in `iton.cs` if the real module turns out to differ.
