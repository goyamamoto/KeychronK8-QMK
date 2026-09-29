# K8 Bluetooth regression tests under Renode. Each test boots a fresh K8
# with the model Bluetooth module (iton.cs) and checks what reaches the
# module and the host. Run with ./run.sh (see README.md).
from System import Environment
execfile(Environment.GetEnvironmentVariable("K8_SIM_DIR") + "/k8test.py")

R = Results()

HID_NONE = "HID 00 00 00 00 00 00 00 00"
HID_A = "HID 00 00 04 00 00 00 00 00"

# rgb.c bt_state_t
BT_IDLE, BT_CONNECTING, BT_PAIRING, BT_CONNECTED, BT_DISCONNECTED = range(5)
# ChibiOS usbstate_t
USB_READY, USB_SUSPENDED = 2, 5


def connected_k8(**kw):
    k = K8(bonded=[0], **kw)
    k.run(1.5)
    return k


def test_boot_bluetooth_win():
    k = K8()
    k.run(1.0)
    p = k.packets()
    R.check("boot: switch to profile 1", len(p) >= 1 and p[0] == "A6 51 81", str(p))
    R.check("boot: OS Windows after the switch", len(p) >= 2 and p[1] == "A6 51 75", str(p))
    if len(p) >= 2:
        gap = k.iton.PacketTime(1) - k.iton.PacketTime(0)
        R.check("boot: OS command 300 ms after the switch", gap >= 300, "%.1f ms" % gap)
    R.check("boot: module has the OS", k.iton.Os == "win", k.iton.Os)
    R.check("boot: not connected without a bond", not k.iton.Connected and k.u8("iton_bt_is_connected") == 0)
    k.close()


def test_boot_mac():
    k = K8(mac=True)
    k.run(1.0)
    p = k.packets()
    R.check("boot Mac: OS Mac after the switch", p[:2] == ["A6 51 81", "A6 51 74"], str(p))
    k.close()


def test_boot_cable():
    k = K8(cable=True)
    k.run(1.0)
    p = k.packets()
    R.check("boot cable: module told USB mode, nothing else", p == ["A6 58 01"], str(p))
    k.close()


def test_connect_and_type():
    k = connected_k8()
    R.check("connect: bonded profile connects", k.iton.Connected and k.u8("iton_bt_is_connected") == 1)
    R.check("connect: keyboard shows connected", k.u8("bt_state") == BT_CONNECTED, str(k.u8("bt_state")))
    n = k.iton.HostCount
    k.tap(KEY_A)
    h = k.host(n)
    R.check("typing: A down then up", h == [HID_A, HID_NONE], str(h))
    k.close()


def test_six_keys():
    k = connected_k8()
    n = k.iton.HostCount
    keys = [KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H]
    for key in keys:
        k.press(key)
        k.run(0.05)
    for key in reversed(keys):
        k.release(key)
        k.run(0.05)
    h = k.host(n)
    # Five keys in the HID report, the sixth (H = 0x0B) as a bitmap.
    R.check("6 keys: five in the HID report",
            len(h) >= 5 and h[4] == "HID 00 00 04 16 07 09 0A 00", str(h[:6]))
    R.check("6 keys: sixth as NKRO bit 0x0B",
            len(h) >= 6 and h[5] == "NKRO 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00", str(h[4:7]))
    R.check("6 keys: all released at the end", len(h) > 0 and h[-1] == HID_NONE, str(h[-2:]))
    k.close()


def test_consumer():
    k = connected_k8()
    n = k.iton.HostCount
    k.fn_tap(VOLU)
    h = k.host(n)
    R.check("Fn+F12: volume up then release", h == ["CONSUMER 00 E9", "CONSUMER 00 00"], str(h))
    k.close()


def test_host_leds():
    k = connected_k8()
    k.iton.HostSetLeds(0x02)
    k.run(0.1)
    R.check("host LEDs: Caps Lock state stored", k.u8("iton_bt_led_state") == 0x02, str(k.u8("iton_bt_led_state")))
    R.check("host LEDs: Caps Lock LED on", k.gpio.Level(CAPS_LED))
    k.iton.HostSetLeds(0x00)
    k.run(0.1)
    R.check("host LEDs: Caps Lock LED off again", not k.gpio.Level(CAPS_LED))
    k.close()


def test_battery():
    k = connected_k8()
    k.iton.SendNotification(0x5A, 0x04)
    k.run(0.1)
    R.check("battery: a level the module sends unasked is not shown", k.u8("battery_level") == 0,
            str(k.u8("battery_level")))
    n = k.iton.PacketsFromMcu
    k.iton.BatteryLevel = 0x02
    k.fn_tap(KEY_B)
    k.run(0.1)
    R.check("Fn+B: battery query sent", "A6 51 61" in k.packets(n), str(k.packets(n)))
    R.check("Fn+B: level stored", k.u8("battery_level") == 0x02, str(k.u8("battery_level")))
    k.iton.SendNotification(0x5A, 0x06)
    k.run(0.1)
    R.check("low battery: flagged", k.u8("battery_low") == 1)
    k.iton.SendNotification(0x5A, 0x0A)
    k.run(0.1)
    R.check("low battery: cleared", k.u8("battery_low") == 0)
    k.close()


def test_pairing():
    k = K8()
    k.run(1.0)
    n = k.iton.PacketsFromMcu
    k.press(FN)
    k.run(0.03)
    k.press(KEY_1)
    k.run(2.5)
    R.check("pairing: not before 3 s", "A6 51 89" not in k.packets(n), str(k.packets(n)))
    k.run(0.7)
    k.release(KEY_1)
    k.release(FN)
    k.run(0.2)
    p = k.packets(n)
    R.check("pairing: Fn+1 selects profile 1", len(p) >= 1 and p[0] == "A6 51 81", str(p))
    R.check("pairing: held 3 s enters pairing", p.count("A6 51 89") == 1, str(p))
    R.check("pairing: keyboard shows pairing", k.u8("bt_state") == BT_PAIRING, str(k.u8("bt_state")))
    k.iton.HostPair()
    k.run(0.2)
    R.check("pairing: host pairs, connected", k.u8("iton_bt_is_connected") == 1 and k.u8("bt_state") == BT_CONNECTED)
    n = k.iton.HostCount
    k.tap(KEY_A)
    R.check("pairing: typing reaches the new host", k.host(n) == [HID_A, HID_NONE], str(k.host(n)))
    k.close()


def test_profile_switch():
    k = connected_k8()
    n = k.iton.PacketsFromMcu
    k.fn_tap(KEY_2)
    k.run(0.2)
    p = k.packets(n)
    R.check("Fn+2: switch to profile 2", p == ["A6 51 82"], str(p))
    R.check("Fn+2: profile saved", k.u8("k8_config") == 1, str(k.u8("k8_config")))
    R.check("Fn+2: old link dropped", not k.iton.Connected and k.iton.Profile == 1)
    k.close()


def test_disconnect():
    k = connected_k8()
    k.iton.HostDisconnect()
    k.run(0.1)
    R.check("disconnect: keyboard shows it", k.u8("iton_bt_is_connected") == 0 and k.u8("bt_state") == BT_DISCONNECTED)
    k.close()


def test_link_changes_on_their_own():
    k = connected_k8()
    R.check("link: connecting after start-up is shown", k.u8("link_outcome_shown") == 1)
    k.tap(KEY_A)
    k.run(15.0)
    k.tap(KEY_A)
    k.run(15.0)
    k.iton.HostDisconnect()
    k.run(0.1)
    R.check("link: a drop long after any action is not shown",
            k.u8("bt_state") == BT_DISCONNECTED and k.u8("link_outcome_shown") == 0)
    k.fn_tap(KEY_1)
    k.run(1.5)
    R.check("link: the outcome of Fn+1 is shown",
            k.u8("bt_state") == BT_CONNECTED and k.u8("link_outcome_shown") == 1, str(k.u8("bt_state")))
    k.close()


def test_switch_to_cable_and_back():
    k = connected_k8()
    n = k.iton.PacketsFromMcu
    k.set_dip(False, True)
    k.run(0.3)
    R.check("switch to cable: USB mode", k.packets(n) == ["A6 58 01"], str(k.packets(n)))
    n = k.iton.PacketsFromMcu
    k.set_dip(False, False)
    k.run(1.5)
    p = k.packets(n)
    R.check("switch to Bluetooth: profile then OS", p[:2] == ["A6 51 81", "A6 51 75"], str(p))
    R.check("switch to Bluetooth: reconnects", k.iton.Connected)
    k.close()


def test_unresponsive_module():
    k = connected_k8()
    k.iton.Responsive = False
    n = k.iton.HostCount
    k.tap(KEY_A)
    k.tap(KEY_S)
    k.run(0.3)
    R.check("stuck module: nothing reaches the host", k.host(n) == [], str(k.host(n)))
    k.iton.Responsive = True
    k.run(0.1)
    n = k.iton.HostCount
    k.tap(KEY_A)
    R.check("stuck module: typing works once it answers", k.host(n)[-2:] == [HID_A, HID_NONE], str(k.host(n)))
    k.close()


def test_sleep_and_wake():
    k = connected_k8()
    k.run(21.0)
    R.check("sleep: deep sleep after the idle time", k.pmu.DeepSleep and k.pmu.DeepSleepEntries >= 1,
            "entries=%d" % k.pmu.DeepSleepEntries)
    k.press(KEY_A)
    k.run(0.1)
    R.check("sleep: a key wakes the MCU", not k.pmu.DeepSleep)
    k.release(KEY_A)
    k.run(0.3)
    R.check("sleep: link kept", k.iton.Connected and k.u8("iton_bt_is_connected") == 1)
    n = k.iton.HostCount
    k.tap(KEY_A)
    R.check("sleep: typing after wake", k.host(n) == [HID_A, HID_NONE], str(k.host(n)))
    entries = k.pmu.DeepSleepEntries
    k.run(5.0)
    R.check("sleep: idle time starts afresh after wake", k.pmu.DeepSleepEntries == entries)
    k.close()


def test_no_sleep_while_pairing():
    k = K8()
    k.run(1.0)
    k.press(FN)
    k.run(0.03)
    k.press(KEY_1)
    k.run(3.2)
    k.release(KEY_1)
    k.release(FN)
    k.run(22.0)
    R.check("sleep: not while pairing", k.pmu.DeepSleepEntries == 0 and k.u8("bt_state") == BT_PAIRING,
            "entries=%d state=%d" % (k.pmu.DeepSleepEntries, k.u8("bt_state")))
    k.iton.HostPair()
    k.run(0.2)
    R.check("sleep: pairing late still shows connected", k.u8("bt_state") == BT_CONNECTED, str(k.u8("bt_state")))
    k.run(21.0)
    R.check("sleep: sleeps once connected", k.pmu.DeepSleepEntries >= 1)
    k.close()


def test_no_sleep_with_usb_host():
    k = connected_k8(usb_host=True)
    k.run(21.0)
    R.check("sleep: not while a USB host sends frames", k.pmu.DeepSleepEntries == 0)
    k.close()


def test_no_sleep_on_cable():
    k = K8(cable=True)
    k.run(22.0)
    R.check("sleep: never on the cable", k.pmu.DeepSleepEntries == 0)
    k.close()


def test_caps_change_while_asleep():
    k = connected_k8()
    k.iton.HostSetLeds(0x02)
    k.run(0.1)
    k.run(21.0)
    asleep = k.pmu.DeepSleep
    k.iton.HostSetLeds(0x00)     # e.g. Caps Lock turned off on the Mac
    k.run(0.1)
    k.tap(KEY_A)
    k.run(0.5)
    R.check("sleep+LEDs: was asleep when the host changed Caps Lock", asleep)
    R.known_issue("sleep+LEDs: Caps Lock LED follows the host after wake", not k.gpio.Level(CAPS_LED),
                  "LED still on: the module's B1 packet came while the MCU slept (lost=%d)" % k.iton.LostToMcu)
    k.close()


def test_usb_suspend_in_bluetooth_mode():
    k = connected_k8(usb_host=True)
    k.usb.Suspend()
    k.run(0.2)
    suspended = k.u8("USBD1") == USB_SUSPENDED
    n = k.iton.HostCount
    k.tap(KEY_A)
    k.run(0.3)
    R.check("USB suspend: USB driver suspended", suspended, str(k.u8("USBD1")))
    # With Bluetooth, QMK builds without the USB suspend wait in
    # protocol_pre_task (NO_USB_STARTUP_CHECK).
    R.check("USB suspend: Bluetooth typing goes on", k.host(n) == [HID_A, HID_NONE], str(k.host(n)))
    k.close()


test_boot_bluetooth_win()
test_boot_mac()
test_boot_cable()
test_connect_and_type()
test_six_keys()
test_consumer()
test_host_leds()
test_battery()
test_pairing()
test_profile_switch()
test_disconnect()
test_link_changes_on_their_own()
test_switch_to_cable_and_back()
test_unresponsive_module()
test_sleep_and_wake()
test_no_sleep_while_pairing()
test_no_sleep_with_usb_host()
test_no_sleep_on_cable()
test_caps_change_while_asleep()
test_usb_suspend_in_bluetooth_mode()
R.summary()
