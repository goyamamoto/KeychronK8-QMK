// SN32F248B peripheral models for running the Keychron K8 firmware under
// Renode. Each model covers what the SonixQMK ChibiOS HAL and the K8 code
// use, no more. Register layouts follow SN32F240B.h in ChibiOS-Contrib.
// Loaded with `include @sn32.cs` before the platform description.

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
    // Plain storage: reads return what was written. For SYS1, PFPA, WDT and
    // other blocks whose init code only writes enable bits.
    public class SN32RegFile : IDoubleWordPeripheral, IKnownSize
    {
        public SN32RegFile(IMachine machine, long size = 0x100)
        {
            Size = size;
        }

        public long Size { get; }

        public uint ReadDoubleWord(long offset)
        {
            return regs.TryGetValue(offset, out var v) ? v : 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            regs[offset] = value;
        }

        public void Reset()
        {
            regs.Clear();
        }

        private readonly Dictionary<long, uint> regs = new Dictionary<long, uint>();
    }

    // SYS0: every clock source reports ready at once and the clock switch
    // status follows the selection.
    public class SN32Sys0 : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Sys0(IMachine machine) { }

        public long Size => 0x100;

        public uint ReadDoubleWord(long offset)
        {
            switch(offset)
            {
            case 0x08: // CSST: IHRC, ILRC, EHSXTAL, ELSXTAL, PLL ready
                return 0x1F;
            case 0x0C: // CLKCFG: SYSCLKST [6:4] follows SYSCLKSEL [2:0]
                var v = Get(offset);
                return (v & ~0x70u) | ((v & 0x7) << 4);
            default:
                return Get(offset);
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            regs[offset] = value;
        }

        public void Reset()
        {
            regs.Clear();
        }

        private uint Get(long offset)
        {
            return regs.TryGetValue(offset, out var v) ? v : 0;
        }

        private readonly Dictionary<long, uint> regs = new Dictionary<long, uint>();
    }

    // PMU: CTRL 0x2 before WFI means deep sleep, where peripheral clocks stop.
    public class SN32Pmu : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Pmu(IMachine machine) { }

        public long Size => 0x100;

        public bool DeepSleep => ctrl == 0x2;
        public int DeepSleepEntries { get; private set; }

        public uint ReadDoubleWord(long offset)
        {
            return offset == 0x40 ? ctrl : 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            if(offset == 0x40)
            {
                if(value == 0x2 && ctrl != 0x2)
                {
                    DeepSleepEntries++;
                    this.Log(LogLevel.Info, "deep sleep");
                }
                else if(value != 0x2 && ctrl == 0x2)
                {
                    this.Log(LogLevel.Info, "awake");
                }
                ctrl = value;
            }
        }

        public void Reset()
        {
            ctrl = 0;
            DeepSleepEntries = 0;
        }

        private uint ctrl;
    }

    // Flash controller. The HAL programs one word per START (ADDR, DATA,
    // CTRL = PG | START) and erases 64-byte pages (CTRL = PER, ADDR, START).
    public class SN32Flash : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Flash(IMachine machine)
        {
            this.machine = machine;
        }

        public long Size => 0x100;
        public int Programs { get; private set; }
        public int Erases { get; private set; }

        public uint ReadDoubleWord(long offset)
        {
            switch(offset)
            {
            case 0x00: return lpctrl;
            case 0x04: return 0; // STATUS: never busy, no error
            case 0x08: return ctrl;
            case 0x0C: return data;
            case 0x10: return addr;
            default: return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch(offset)
            {
            case 0x00: lpctrl = value; break;
            case 0x08:
                ctrl = value & ~StartBit;
                if((value & StartBit) != 0)
                {
                    Start();
                }
                break;
            case 0x0C: data = value; break;
            case 0x10: addr = value; break;
            }
        }

        public void Reset()
        {
            lpctrl = ctrl = data = addr = 0;
        }

        private void Start()
        {
            var bus = machine.GetSystemBus(this);
            if((ctrl & PgBit) != 0)
            {
                var a = addr & ~3u;
                // NOR flash: programming only clears bits.
                bus.WriteDoubleWord(a, bus.ReadDoubleWord(a) & data);
                Programs++;
            }
            else if((ctrl & PerBit) != 0)
            {
                var page = addr & ~63u;
                for(uint i = 0; i < 64; i += 4)
                {
                    bus.WriteDoubleWord(page + i, 0xFFFFFFFF);
                }
                Erases++;
            }
        }

        private const uint PgBit = 0x1;
        private const uint PerBit = 0x2;
        private const uint StartBit = 0x40;

        private readonly IMachine machine;
        private uint lpctrl, ctrl, data, addr;
    }

    // All four GPIO ports (P0..P3 = A..D, 0x2000 apart) plus what is wired to
    // them: the key matrix and lines driven by other chips.
    //
    // Key matrix (COL2ROW diodes, from column to row): a pressed key pulls
    // its column low while its row is driven low, and drives its row high
    // while its column is driven high. Floating row pins read low (the board
    // pulls them down); other floating pins read high.
    public class SN32Gpio : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Gpio(IMachine machine, string rowPins, string colPins)
        {
            rows = rowPins.Split(',').Select(ParsePin).ToArray();
            cols = colPins.Split(',').Select(ParsePin).ToArray();
            for(var i = 0; i < Ports; i++)
            {
                irqs[i] = new GPIO();
            }
            Reset();
        }

        public long Size => 0x8000;

        public GPIO IRQA => irqs[0];
        public GPIO IRQB => irqs[1];
        public GPIO IRQC => irqs[2];
        public GPIO IRQD => irqs[3];

        // Raised with (pin, level) whenever a pin's level changes.
        public event Action<int, bool> PinChanged;

        public void Press(int row, int col)
        {
            pressed.Add(Tuple.Create(row, col));
            Update();
        }

        public void Release(int row, int col)
        {
            pressed.Remove(Tuple.Create(row, col));
            Update();
        }

        public void ReleaseAll()
        {
            pressed.Clear();
            Update();
        }

        // Another chip drives the pin (e.g. "A1").
        public void Drive(string pin, bool level)
        {
            external[ParsePin(pin)] = level;
            Update();
        }

        public void Undrive(string pin)
        {
            external.Remove(ParsePin(pin));
            Update();
        }

        public bool Level(string pin)
        {
            return Level(ParsePin(pin));
        }

        public bool Level(int pin)
        {
            return ((levels[pin >> 4] >> (pin & 15)) & 1) != 0;
        }

        public bool IsOutput(string pin)
        {
            var p = ParsePin(pin);
            return ((mode[p >> 4] >> (p & 15)) & 1) != 0;
        }

        public uint ReadDoubleWord(long offset)
        {
            var port = (int)(offset >> 13);
            if(port >= Ports)
            {
                return 0;
            }
            switch(offset & 0x1FFF)
            {
            case 0x00: return levels[port];
            case 0x04: return mode[port];
            case 0x08: return cfg[port];
            case 0x0C: return isense[port];
            case 0x10: return ibs[port];
            case 0x14: return iev[port];
            case 0x18: return ie[port];
            case 0x1C: return ris[port];
            case 0x2C: return cfg1[port];
            default: return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            var port = (int)(offset >> 13);
            if(port >= Ports)
            {
                return;
            }
            switch(offset & 0x1FFF)
            {
            case 0x00: latch[port] = value & 0xFFFF; break;
            case 0x04: mode[port] = value & 0xFFFF; break;
            case 0x08: cfg[port] = value; break;
            case 0x0C: isense[port] = value; break;
            case 0x10: ibs[port] = value; break;
            case 0x14: iev[port] = value; break;
            case 0x18: ie[port] = value; UpdateIrq(port); return;
            case 0x20: ris[port] &= ~value; UpdateIrq(port); return;
            case 0x24: latch[port] |= value & 0xFFFF; break;
            case 0x28: latch[port] &= ~value; break;
            case 0x2C: cfg1[port] = value; break;
            default: return;
            }
            Update();
        }

        public void Reset()
        {
            for(var i = 0; i < Ports; i++)
            {
                latch[i] = mode[i] = cfg[i] = cfg1[i] = isense[i] = ibs[i] = iev[i] = ie[i] = ris[i] = 0;
                levels[i] = 0;
            }
            pressed.Clear();
            external.Clear();
            Update(false);
        }

        public static int ParsePin(string name)
        {
            name = name.Trim().ToUpperInvariant();
            return (name[0] - 'A') * 16 + int.Parse(name.Substring(1));
        }

        private bool OutputHigh(int pin)
        {
            return IsOut(pin) && ((latch[pin >> 4] >> (pin & 15)) & 1) != 0;
        }

        private bool OutputLow(int pin)
        {
            return IsOut(pin) && ((latch[pin >> 4] >> (pin & 15)) & 1) == 0;
        }

        private bool IsOut(int pin)
        {
            return ((mode[pin >> 4] >> (pin & 15)) & 1) != 0;
        }

        private bool Compute(int pin)
        {
            if(IsOut(pin))
            {
                return OutputHigh(pin);
            }
            if(external.TryGetValue(pin, out var ext))
            {
                return ext;
            }
            var row = Array.IndexOf(rows, pin);
            var col = Array.IndexOf(cols, pin);
            foreach(var k in pressed)
            {
                if(row >= 0 && k.Item1 == row && OutputHigh(cols[k.Item2]))
                {
                    return true;
                }
                if(col >= 0 && k.Item2 == col && OutputLow(rows[k.Item1]))
                {
                    return false;
                }
            }
            var pad = pin & 15;
            var c = pad < 16 ? (cfg[pin >> 4] >> (pad * 2)) & 3 : 0;
            switch(c)
            {
            case 0: return true;  // pull-up
            case 1: return false; // pull-down
            default: return row < 0;
            }
        }

        private void Update(bool edges = true)
        {
            for(var port = 0; port < Ports; port++)
            {
                uint now = 0;
                for(var pad = 0; pad < 16; pad++)
                {
                    if(Compute(port * 16 + pad))
                    {
                        now |= 1u << pad;
                    }
                }
                var changed = now ^ levels[port];
                levels[port] = now;
                if(!edges || changed == 0)
                {
                    continue;
                }
                var rising = changed & now;
                var falling = changed & ~now;
                // Edge-sensitive pins (IS = 0): both edges (IBS) or the one IEV picks.
                var edge = ~isense[port];
                var hit = edge & ((ibs[port] & changed) | (~ibs[port] & ~iev[port] & rising) | (~ibs[port] & iev[port] & falling));
                ris[port] |= hit & 0xFFFF;
                UpdateIrq(port);
                for(var pad = 0; pad < 16; pad++)
                {
                    if((changed & (1u << pad)) != 0)
                    {
                        PinChanged?.Invoke(port * 16 + pad, (now & (1u << pad)) != 0);
                    }
                }
            }
        }

        private void UpdateIrq(int port)
        {
            irqs[port].Set((ris[port] & ie[port]) != 0);
        }

        private const int Ports = 4;

        private readonly int[] rows;
        private readonly int[] cols;
        private readonly HashSet<Tuple<int, int>> pressed = new HashSet<Tuple<int, int>>();
        private readonly Dictionary<int, bool> external = new Dictionary<int, bool>();
        private readonly GPIO[] irqs = new GPIO[Ports];
        private readonly uint[] latch = new uint[Ports], mode = new uint[Ports], cfg = new uint[Ports], cfg1 = new uint[Ports];
        private readonly uint[] isense = new uint[Ports], ibs = new uint[Ports], iev = new uint[Ports], ie = new uint[Ports], ris = new uint[Ports];
        private readonly uint[] levels = new uint[Ports];
    }

    // CT16B0/CT16B1: 16-bit up counter with prescaler and 25 match registers.
    // MR n: interrupt (MCTRL bit 3n), reset (3n+1), stop (3n+2), ten
    // channels per MCTRL register.
    public class SN32Timer : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Timer(IMachine machine, ulong frequency = 48000000)
        {
            pclk = frequency;
            timer = new LimitTimer(machine.ClockSource, pclk, this, "counter", 1, Direction.Ascending, false, WorkMode.OneShot, true);
            timer.LimitReached += OnLimit;
            IRQ = new GPIO();
        }

        public long Size => 0xAC;
        public GPIO IRQ { get; }

        public uint ReadDoubleWord(long offset)
        {
            switch(offset)
            {
            case 0x00: return tmrctrl;
            case 0x04: return Tc;
            case 0x08: return pre;
            case 0x14: return mctrl[0];
            case 0x18: return mctrl[1];
            case 0x1C: return mctrl[2];
            case 0xA4: return ris;
            default:
                if(offset >= 0x20 && offset <= 0x80)
                {
                    return mr[(offset - 0x20) / 4];
                }
                return other.TryGetValue(offset, out var v) ? v : 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            Sync();
            switch(offset)
            {
            case 0x00:
                tmrctrl = value & 1;
                if((value & 2) != 0) // CRST
                {
                    tc = 0;
                }
                break;
            case 0x04: tc = value & 0xFFFF; break;
            case 0x08: pre = value & 0xFFFF; break;
            case 0x14: mctrl[0] = value; break;
            case 0x18: mctrl[1] = value; break;
            case 0x1C: mctrl[2] = value; break;
            case 0xA8: ris &= ~value; UpdateIrq(); break;
            default:
                if(offset >= 0x20 && offset <= 0x80)
                {
                    mr[(offset - 0x20) / 4] = value & 0xFFFF;
                }
                else
                {
                    other[offset] = value;
                }
                break;
            }
            Schedule();
        }

        public void Reset()
        {
            timer.Reset();
            tmrctrl = tc = pre = ris = 0;
            Array.Clear(mctrl, 0, mctrl.Length);
            Array.Clear(mr, 0, mr.Length);
            other.Clear();
            UpdateIrq();
        }

        private uint Tc => timer.Enabled ? (uint)((tc + timer.Value) & 0xFFFF) : tc;

        private uint Bits(int ch)
        {
            return (mctrl[ch / 10] >> ((ch % 10) * 3)) & 7;
        }

        // Fold the ticks counted so far into tc.
        private void Sync()
        {
            if(timer.Enabled)
            {
                tc = (uint)((tc + timer.Value) & 0xFFFF);
                timer.Enabled = false;
            }
        }

        // Run the counter up to the nearest match (or the 16-bit wrap).
        private void Schedule()
        {
            if((tmrctrl & 1) == 0)
            {
                return;
            }
            ulong next = 0x10000;
            for(var ch = 0; ch < 25; ch++)
            {
                if(Bits(ch) == 0)
                {
                    continue;
                }
                var d = (mr[ch] - tc) & 0xFFFF;
                if(d == 0)
                {
                    d = 0x10000;
                }
                next = Math.Min(next, d);
            }
            timer.Frequency = pclk / (pre + 1);
            timer.Value = 0;
            timer.Limit = next;
            timer.Enabled = true;
        }

        private void OnLimit()
        {
            tc = (uint)((tc + timer.Limit) & 0xFFFF);
            timer.Enabled = false;
            var reset = false;
            for(var ch = 0; ch < 25; ch++)
            {
                var b = Bits(ch);
                if(b == 0 || mr[ch] != tc)
                {
                    continue;
                }
                ris |= 1u << ch;
                reset |= (b & 2) != 0;
                if((b & 4) != 0)
                {
                    tmrctrl &= ~1u;
                }
            }
            if(reset)
            {
                tc = 0;
            }
            UpdateIrq();
            Schedule();
        }

        private void UpdateIrq()
        {
            uint enabled = 0;
            for(var ch = 0; ch < 25; ch++)
            {
                if((Bits(ch) & 1) != 0)
                {
                    enabled |= 1u << ch;
                }
            }
            IRQ.Set((ris & enabled) != 0);
        }

        private readonly LimitTimer timer;
        private readonly ulong pclk;
        private uint tmrctrl, tc, pre, ris;
        private readonly uint[] mctrl = new uint[3];
        private readonly uint[] mr = new uint[25];
        private readonly Dictionary<long, uint> other = new Dictionary<long, uint>();
    }

    // SPI0 as used by the K8: the MCU is the slave. The master (the
    // Bluetooth module model) calls Exchange for every byte it clocks.
    public class SN32Spi : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Spi(IMachine machine)
        {
            IRQ = new GPIO();
        }

        public long Size => 0x24;
        public GPIO IRQ { get; }

        public bool Enabled => (ctrl0 & 1) != 0;
        public bool Slave => (ctrl0 & 8) != 0;
        public int TxCount => tx.Count;
        public int RxCount => rx.Count;
        public int Underruns { get; private set; }
        public int Overflows { get; private set; }

        // One byte from the master; returns the byte the slave shifts out.
        public byte Exchange(byte fromMaster)
        {
            byte toMaster = 0;
            if(tx.Count > 0)
            {
                toMaster = tx.Dequeue();
            }
            else
            {
                Underruns++;
            }
            if(rx.Count < FifoDepth)
            {
                rx.Enqueue(fromMaster);
            }
            else
            {
                Overflows++;
            }
            UpdateIrq();
            return toMaster;
        }

        public uint ReadDoubleWord(long offset)
        {
            switch(offset)
            {
            case 0x00: return ctrl0;
            case 0x04: return ctrl1;
            case 0x08: return clkdiv;
            case 0x0C:
                uint stat = 0;
                if(tx.Count == 0) stat |= 1;
                if(tx.Count >= FifoDepth) stat |= 2;
                if(rx.Count == 0) stat |= 4;
                if(rx.Count >= FifoDepth) stat |= 8;
                if(RxThreshold) stat |= 0x40;
                return stat;
            case 0x10: return ie;
            case 0x14: return Ris;
            case 0x1C:
                var b = rx.Count > 0 ? rx.Dequeue() : (byte)0;
                UpdateIrq();
                return b;
            case 0x20: return dfdly;
            default: return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch(offset)
            {
            case 0x00:
                if((value & 0xC0) != 0) // FRESET
                {
                    tx.Clear();
                    rx.Clear();
                }
                ctrl0 = value & ~0xC0u;
                break;
            case 0x04: ctrl1 = value; break;
            case 0x08: clkdiv = value; break;
            case 0x10: ie = value; break;
            case 0x18: txThresholdFlag = txThresholdFlag && (value & 8) == 0; break;
            case 0x1C:
                if(tx.Count < FifoDepth)
                {
                    tx.Enqueue((byte)value);
                }
                break;
            case 0x20: dfdly = value; break;
            }
            UpdateIrq();
        }

        public void Reset()
        {
            ctrl0 = ctrl1 = clkdiv = ie = dfdly = 0;
            tx.Clear();
            rx.Clear();
            Underruns = Overflows = 0;
            UpdateIrq();
        }

        private int RxLevel => (int)((ctrl0 >> 15) & 7);
        // Level flag: set while the RX FIFO holds more than RXFIFOTH bytes.
        private bool RxThreshold => rx.Count > RxLevel;
        private uint Ris => (RxThreshold ? 4u : 0u) | (txThresholdFlag ? 8u : 0u);

        private void UpdateIrq()
        {
            IRQ.Set((Ris & ie) != 0);
        }

        private const int FifoDepth = 8;
        private uint ctrl0, ctrl1, clkdiv, ie, dfdly;
        private bool txThresholdFlag;
        private readonly Queue<byte> tx = new Queue<byte>();
        private readonly Queue<byte> rx = new Queue<byte>();
    }

    // USB device: no enumeration, only what the K8 code looks at. A host,
    // when present, moves the frame number every millisecond; Suspend() and
    // Resume() raise the bus events the HAL turns into USB_SUSPENDED/_wakeup.
    public class SN32Usb : IDoubleWordPeripheral, IKnownSize
    {
        public SN32Usb(IMachine machine)
        {
            IRQ = new GPIO();
            sof = new LimitTimer(machine.ClockSource, 1000, this, "sof", 1, Direction.Ascending, false, WorkMode.Periodic, true);
            sof.LimitReached += () => frame = (frame + 1) & 0x7FF;
        }

        public long Size => 0x100;
        public GPIO IRQ { get; }

        public bool HostPresent
        {
            get => sof.Enabled;
            set => sof.Enabled = value;
        }

        public void Suspend()
        {
            HostPresent = false;
            Raise(BusSuspend);
        }

        public void Resume()
        {
            HostPresent = true;
            Raise(BusResume);
        }

        public uint ReadDoubleWord(long offset)
        {
            switch(offset)
            {
            case 0x00: return inten;
            case 0x04: return insts;
            case 0x60: return frame;
            case 0x80: return 0; // RWSTATUS: never busy
            case 0x8C: return 0;
            default: return regs.TryGetValue(offset, out var v) ? v : 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch(offset)
            {
            case 0x00: inten = value; break;
            case 0x08: insts &= ~value; break; // INSTSC
            default: regs[offset] = value; break;
            }
            UpdateIrq();
        }

        public void Reset()
        {
            inten = insts = frame = 0;
            regs.Clear();
            sof.Reset();
            UpdateIrq();
        }

        private void Raise(uint bits)
        {
            insts |= bits;
            UpdateIrq();
        }

        private void UpdateIrq()
        {
            var busEvents = insts & (BusReset | BusSuspend | BusResume | BusWakeup);
            IRQ.Set(busEvents != 0 && (inten & BusIe) != 0);
        }

        private const uint BusWakeup = 1u << 25;
        private const uint BusResume = 1u << 29;
        private const uint BusSuspend = 1u << 30;
        private const uint BusReset = 1u << 31;
        private const uint BusIe = 1u << 31;

        private readonly LimitTimer sof;
        private uint inten, insts, frame;
        private readonly Dictionary<long, uint> regs = new Dictionary<long, uint>();
    }
}
