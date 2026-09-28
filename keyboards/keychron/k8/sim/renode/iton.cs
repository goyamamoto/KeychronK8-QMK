// Model of the K8's Bluetooth module for Renode, as seen from the MCU.
//
// Wiring (drivers/bluetooth/iton_bt.c): SPI0 with the module as master, A0
// ("request", driven by the MCU: high while a packet waits for the module to
// clock it out) and A1 ("direction", driven by the module: high while it
// clocks a packet in to the MCU). Packet codes are the ones in iton_bt.h.
//
// Timings and the reactions below are the model's choices, picked to match
// what the K8 does on the desk; they are not measured:
//   * the module starts clocking RequestLatencyUs after A0 rises and clocks
//     one byte every ByteGapUs until A0 falls;
//   * after a profile switch it ignores packets for BusyAfterSwitchMs
//     (on the K8 the OS command was lost when sent right after the switch);
//   * a bonded profile connects ConnectMs after the switch;
//   * packets from the module are lost while the MCU is in deep sleep,
//     because the SPI block has no clock then.
//
// The test scripts drive the host side (HostPair, HostSetLeds, ...) and read
// what the host saw from the log (lines starting with "ITON").

using System;
using System.Collections.Generic;
using System.Linq;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Timers;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.K8
{
    public class ItonModule : IDoubleWordPeripheral, IKnownSize
    {
        public ItonModule(IMachine machine, SN32Gpio gpio, SN32Spi spi, SN32Pmu pmu)
        {
            this.machine = machine;
            this.gpio = gpio;
            this.spi = spi;
            this.pmu = pmu;
            clock = new LimitTimer(machine.ClockSource, 1000000, this, "clock", 1, Direction.Ascending, false, WorkMode.OneShot, true);
            clock.LimitReached += OnClock;
            gpio.PinChanged += OnPin;
            Reset();
        }

        // Not on the MCU's bus; the address only gives the monitor a name.
        public long Size => 0x4;
        public uint ReadDoubleWord(long offset) { return 0; }
        public void WriteDoubleWord(long offset, uint value) { }

        public int RequestLatencyUs { get; set; } = 100;
        public int ByteGapUs { get; set; } = 20;
        public int BusyAfterSwitchMs { get; set; } = 200;
        public int ConnectMs { get; set; } = 800;
        // false: the module stops clocking packets out of the MCU. Back to
        // true, it serves a request that is still pending.
        public bool Responsive
        {
            get => responsive;
            set
            {
                responsive = value;
                if(value)
                {
                    CheckRequest();
                }
            }
        }
        public byte BatteryLevel { get; set; } = 0x04;

        public string Mode { get; private set; }
        public int Profile { get; private set; }
        public string Os { get; private set; }
        public bool Connected { get; private set; }
        public bool Pairing { get; private set; }
        public int PacketsFromMcu => fromMcu.Count;
        public int LostToMcu { get; private set; }

        // Every packet the MCU sent, oldest first, as hex ("A6 51 81").
        public string Packet(int i)
        {
            return i < fromMcu.Count ? Hex(fromMcu[i]) : "";
        }

        public string Packets(int from)
        {
            return string.Join(" | ", fromMcu.Skip(from).Select(Hex));
        }

        // Milliseconds of virtual time at which packet i arrived.
        public double PacketTime(int i)
        {
            return i < fromMcuTimes.Count ? fromMcuTimes[i] : -1;
        }

        // What the host saw ("HID 00 00 04 00 00 00 00 00", "CONSUMER 00 E9"),
        // and what the module told the MCU ("MCU B6 51 76"), oldest first.
        public int HostCount => hostLog.Count;

        public string Host(int from)
        {
            return string.Join(" | ", hostLog.Skip(from));
        }

        public void Bond(int profile)
        {
            bonded.Add(profile);
        }

        public void Forget(int profile)
        {
            bonded.Remove(profile);
        }

        // The host in pairing range accepts the module: bond and connect.
        public void HostPair()
        {
            if(!Pairing)
            {
                this.Log(LogLevel.Warning, "ITON host pair ignored: not pairing");
                return;
            }
            Pairing = false;
            bonded.Add(Profile);
            Connect();
        }

        public void HostDisconnect()
        {
            if(Connected)
            {
                Connected = false;
                Notify(0x51, 0x78);
            }
        }

        // The host changes its lock LEDs (bit 1 = Caps Lock).
        public void HostSetLeds(int leds)
        {
            if(Connected)
            {
                SendToMcu(new byte[] { 0xB1, (byte)leds });
            }
        }

        public void SendNotification(int type, int param)
        {
            Notify((byte)type, (byte)param);
        }

        public void Reset()
        {
            clock.Reset();
            events.Clear();
            fromMcu.Clear();
            fromMcuTimes.Clear();
            hostLog.Clear();
            toMcu.Clear();
            bonded.Clear();
            current = null;
            outgoing = null;
            Mode = "none";
            Os = "none";
            Profile = 0;
            Connected = Pairing = false;
            busyUntilUs = 0;
            LostToMcu = 0;
            if(gpio != null)
            {
                gpio.Drive("A1", false);
            }
        }

        private void OnPin(int pin, bool level)
        {
            if(pin == A0 && level)
            {
                CheckRequest();
            }
        }

        // One byte of a packet from the MCU.
        private void ClockFromMcu()
        {
            if(current == null)
            {
                return;
            }
            if(!Responsive)
            {
                current = null;
                return;
            }
            if(!gpio.Level(A0))
            {
                Received(current);
                current = null;
                StartOutgoing();
                return;
            }
            if(current.Count >= 32)
            {
                this.Log(LogLevel.Warning, "ITON runaway packet from MCU: {0}", Hex(current));
                current = null;
                return;
            }
            // Renode runs the CPU in time slices, so the MCU's SPI interrupt
            // may not have queued the next byte yet; a real module does not
            // wait, but a real interrupt takes microseconds. Wait for it.
            if(spi.TxCount == 0 && waits < MaxWaits && !pmu.DeepSleep)
            {
                waits++;
                At(ByteGapUs, ClockFromMcu);
                return;
            }
            waits = 0;
            current.Add(pmu.DeepSleep ? (byte)0xFF : spi.Exchange(0x00));
            At(ByteGapUs, ClockFromMcu);
        }

        private void Received(List<byte> p)
        {
            var packet = p.ToArray();
            fromMcu.Add(packet);
            fromMcuTimes.Add(NowUs / 1000.0);
            var now = NowUs;
            if(now < busyUntilUs)
            {
                this.Log(LogLevel.Info, "ITON ignored (busy) {0}", Hex(packet));
                return;
            }
            this.Log(LogLevel.Info, "ITON from MCU {0}", Hex(packet));
            if(packet.Length == 0)
            {
                return;
            }
            switch(packet[0])
            {
            case 0xA1: // keyboard report: mods, reserved, 6 keys
                HostReport("HID", packet);
                break;
            case 0xA2: // 15-byte key bitmap
                HostReport("NKRO", packet);
                break;
            case 0xA3:
                HostReport("CONSUMER", packet);
                break;
            case 0xA4:
                HostReport("SYSTEM", packet);
                break;
            case 0xA6:
                if(packet.Length >= 3)
                {
                    Control(packet[1], packet[2]);
                }
                break;
            }
        }

        private void HostReport(string kind, byte[] packet)
        {
            if(Mode != "bt" || !Connected)
            {
                this.Log(LogLevel.Info, "ITON host missed {0} {1} (not connected)", kind, Hex(packet.Skip(1)));
                return;
            }
            this.Log(LogLevel.Info, "ITON host got {0} {1}", kind, Hex(packet.Skip(1)));
            hostLog.Add(kind + " " + Hex(packet.Skip(1)));
        }

        private void Control(byte cmd, byte param)
        {
            if(cmd == 0x58 && param == 0x01)
            {
                Mode = "usb";
                Connected = Pairing = false;
                return;
            }
            if(cmd != 0x51)
            {
                return;
            }
            if(param >= 0x81 && param <= 0x85)
            {
                Mode = "bt";
                Profile = param - 0x81;
                Connected = Pairing = false;
                busyUntilUs = NowUs + (ulong)BusyAfterSwitchMs * 1000;
                At(20000, () => Notify(0x51, 0x79)); // looking for the host
                if(bonded.Contains(Profile))
                {
                    var profile = Profile;
                    At(ConnectMs * 1000, () => { if(Mode == "bt" && Profile == profile && !Connected && !Pairing) Connect(); });
                }
                return;
            }
            switch(param)
            {
            case 0x62: Mode = "bt"; break;
            case 0x74: Os = "mac"; break;
            case 0x75: Os = "win"; break;
            case 0x89: // enter pairing
                Pairing = true;
                Connected = false;
                bonded.Remove(Profile);
                Notify(0x51, 0x77);
                break;
            case 0x61: // battery level
                Notify(0x5A, BatteryLevel);
                break;
            }
        }

        private void Connect()
        {
            Connected = true;
            Notify(0x51, 0x76);
        }

        private void Notify(byte type, byte param)
        {
            SendToMcu(new byte[] { 0xB6, type, param });
        }

        private void SendToMcu(byte[] packet)
        {
            toMcu.Enqueue(packet);
            StartOutgoing();
        }

        // Packets to the MCU wait until no packet from the MCU is in flight.
        private void StartOutgoing()
        {
            if(outgoing != null || current != null || toMcu.Count == 0)
            {
                return;
            }
            outgoing = toMcu.Dequeue();
            outgoingIndex = 0;
            outgoingLost = false;
            gpio.Drive("A1", true);
            At(50, ClockToMcu);
        }

        private void ClockToMcu()
        {
            if(outgoingIndex < outgoing.Length)
            {
                if(pmu.DeepSleep || !spi.Enabled)
                {
                    outgoingLost = true;
                }
                else
                {
                    // As above: let the MCU drain its RX FIFO first.
                    if(spi.RxCount > 0 && waits < MaxWaits)
                    {
                        waits++;
                        At(ByteGapUs, ClockToMcu);
                        return;
                    }
                    waits = 0;
                    spi.Exchange(outgoing[outgoingIndex]);
                }
                outgoingIndex++;
                At(ByteGapUs, ClockToMcu);
                return;
            }
            gpio.Drive("A1", false);
            if(outgoingLost)
            {
                LostToMcu++;
                this.Log(LogLevel.Info, "ITON to MCU lost (MCU asleep) {0}", Hex(outgoing));
            }
            else
            {
                this.Log(LogLevel.Info, "ITON to MCU {0}", Hex(outgoing));
            }
            outgoing = null;
            At(ByteGapUs, () => { CheckRequest(); StartOutgoing(); });
        }

        // A0 went high while the module was busy sending.
        private void CheckRequest()
        {
            if(current == null && outgoing == null && gpio.Level(A0))
            {
                current = new List<byte>();
                At(RequestLatencyUs, ClockFromMcu);
            }
        }

        // ----- a small event queue on one timer -----

        private ulong NowUs => (ulong)(machine.LocalTimeSource.ElapsedVirtualTime.TotalMilliseconds * 1000);

        private void At(int delayUs, Action action)
        {
            var due = NowUs + (ulong)Math.Max(1, delayUs);
            events.Add(Tuple.Create(due, seq++, action));
            events.Sort((a, b) => a.Item1 != b.Item1 ? a.Item1.CompareTo(b.Item1) : a.Item2.CompareTo(b.Item2));
            Arm();
        }

        private void Arm()
        {
            clock.Enabled = false;
            if(events.Count == 0)
            {
                return;
            }
            var now = NowUs;
            var due = events[0].Item1;
            clock.Value = 0;
            clock.Limit = due > now ? due - now : 1;
            clock.Enabled = true;
        }

        private void OnClock()
        {
            clock.Enabled = false;
            var now = NowUs;
            while(events.Count > 0 && events[0].Item1 <= now + 1)
            {
                var e = events[0];
                events.RemoveAt(0);
                e.Item3();
            }
            Arm();
        }

        private static string Hex(IEnumerable<byte> bytes)
        {
            return string.Join(" ", bytes.Select(b => b.ToString("X2")));
        }

        private static readonly int A0 = SN32Gpio.ParsePin("A0");

        private readonly IMachine machine;
        private readonly SN32Gpio gpio;
        private readonly SN32Spi spi;
        private readonly SN32Pmu pmu;
        private readonly LimitTimer clock;
        private readonly List<Tuple<ulong, long, Action>> events = new List<Tuple<ulong, long, Action>>();
        private long seq;
        private readonly List<byte[]> fromMcu = new List<byte[]>();
        private readonly List<double> fromMcuTimes = new List<double>();
        private readonly List<string> hostLog = new List<string>();
        private readonly Queue<byte[]> toMcu = new Queue<byte[]>();
        private readonly HashSet<int> bonded = new HashSet<int>();
        private List<byte> current;
        private byte[] outgoing;
        private int outgoingIndex;
        private bool outgoingLost;
        private ulong busyUntilUs;
        private int waits;
        private bool responsive = true;
        private const int MaxWaits = 50;
    }
}
