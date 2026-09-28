# Test helpers for the K8 under Renode (IronPython 2.7, run by the monitor).
# tests.py builds on these; see README.md.
import sys
from System import Environment

# Set by run.sh.
ROOT = Environment.GetEnvironmentVariable("K8_SIM_DIR")
ELF = Environment.GetEnvironmentVariable("K8_ELF")
# K8_SIM_VERBOSE=1 logs the module's traffic.
VERBOSE = Environment.GetEnvironmentVariable("K8_SIM_VERBOSE") == "1"


SYMBOLS = {}
for _line in open(ROOT + "/out/symbols.txt"):
    _name, _addr, _size = _line.split()
    # A name defined twice (two static variables) is ambiguous: refuse it.
    SYMBOLS[_name] = None if _name in SYMBOLS else int(_addr, 16)


def out(line):
    sys.stdout.write(line + "\n")
    sys.stdout.flush()


def sh(cmd):
    monitor.Parse(cmd)


# Matrix positions (row, col) on the ANSI layout.
ESC = (0, 0)
VOLU = (0, 12)      # on the Fn layer
KEY_1 = (1, 1)      # Fn+1: BT_PRF1
KEY_2 = (1, 2)      # Fn+2: BT_PRF2
KEY_A = (3, 1)
KEY_S = (3, 2)
KEY_D = (3, 3)
KEY_F = (3, 4)
KEY_G = (3, 5)
KEY_H = (3, 6)
KEY_B = (4, 6)      # Fn+B: K8_BATT
FN = (5, 12)

CAPS_LED = "B12"
DIP_OS = "D4"       # low = Mac
DIP_CONN = "D5"     # low = Cable (USB)


class K8(object):
    """One emulated K8, created fresh for every test."""

    count = 0

    def __init__(self, mac=False, cable=False, usb_host=False, bonded=(), responsive=True):
        K8.count += 1
        self.name = "k8_%d" % K8.count
        sh('mach create "%s"' % self.name)
        sh('machine LoadPlatformDescription @%s/k8.repl' % ROOT)
        sh('sysbus LoadBinary @%s/ff.bin 0x0' % ROOT)
        sh('sysbus LoadELF @%s' % ELF)
        self.m = monitor.Machine
        if VERBOSE:
            sh('logLevel 1 sysbus.iton')
            sh('logLevel 1 sysbus.pmu')
            out("---- %s" % self.name)
        self.bus = self.m.SystemBus
        self.iton = self.m["sysbus.iton"]
        self.gpio = self.m["sysbus.gpio"]
        self.usb = self.m["sysbus.usb"]
        self.pmu = self.m["sysbus.pmu"]
        for p in bonded:
            self.iton.Bond(p)
        self.iton.Responsive = responsive
        self.usb.HostPresent = usb_host
        self.set_dip(mac, cable)

    def close(self):
        sh('mach rem "%s"' % self.name)

    def set_dip(self, mac, cable):
        self.gpio.Drive(DIP_OS, not mac)
        self.gpio.Drive(DIP_CONN, not cable)

    def run(self, seconds):
        sh('emulation RunFor "%f"' % seconds)

    def now_ms(self):
        return self.m.LocalTimeSource.ElapsedVirtualTime.TotalMilliseconds

    # --- keys ---
    def press(self, key):
        self.gpio.Press(key[0], key[1])

    def release(self, key):
        self.gpio.Release(key[0], key[1])

    def tap(self, key, hold=0.05, after=0.05):
        self.press(key)
        self.run(hold)
        self.release(key)
        self.run(after)

    def fn_tap(self, key, hold=0.05, after=0.1):
        self.press(FN)
        self.run(0.03)
        self.tap(key, hold, 0.03)
        self.release(FN)
        self.run(after)

    # --- firmware state ---
    def sym(self, name):
        addr = SYMBOLS[name]
        if addr is None:
            raise Exception("symbol %s is defined more than once" % name)
        return addr

    def u8(self, name):
        return self.bus.ReadByte(self.sym(name))

    def packets(self, start=0):
        return [self.iton.Packet(i) for i in range(start, self.iton.PacketsFromMcu)]

    def host(self, start=0):
        s = self.iton.Host(start)
        return s.split(" | ") if s else []


class Results(object):
    def __init__(self):
        self.passed = []
        self.failed = []
        self.known = []

    def check(self, name, ok, detail=""):
        if ok:
            self.passed.append(name)
            out("PASS  %s" % name)
        else:
            self.failed.append(name)
            out("FAIL  %s  %s" % (name, detail))

    # A known issue: reported, does not fail the run. If it starts
    # passing, the issue may be fixed and the test should become a check.
    def known_issue(self, name, ok, detail=""):
        if ok:
            self.passed.append(name)
            out("PASS  %s  (known issue no longer shows; turn this into a check)" % name)
        else:
            self.known.append(name)
            out("KNOWN %s  %s" % (name, detail))

    def summary(self):
        out("RESULT pass=%d fail=%d known=%d" % (len(self.passed), len(self.failed), len(self.known)))
