#!/usr/bin/env python3
"""elxie — command-line client for the Elxie Code firmware (USB serial or BLE).

Reference implementation of spec/03-protocol.md; the app's packages/protocol
must behave the same way.

    pip install pyserial bleak
    python tools/elxie_cli.py --port /dev/ttyACM0 info
    python tools/elxie_cli.py --port COM5 upload spec/test-vectors/03_parallel.elxa --slot 1 --run
    python tools/elxie_cli.py --ble info              # first robot named "Elxie-*"
    python tools/elxie_cli.py --ble Elxie-3F2A list
    python tools/elxie_cli.py --port /dev/ttyACM0 telemetry
Commands: info, list, upload FILE [--slot N] [--run], run N, stop, delete N,
          autorun N|none, telemetry, trace, calibrate line|gyro|compass
"""
import argparse
import asyncio
import os
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elxasm  # noqa: E402

SERVICE = "8d3a0001-5e1c-4a8e-9b2d-e1a1e0c0de01"
RX = "8d3a0002-5e1c-4a8e-9b2d-e1a1e0c0de01"   # we write here
TX = "8d3a0003-5e1c-4a8e-9b2d-e1a1e0c0de01"   # robot notifies here

CMD = dict(HELLO=0x01, PING=0x02, UPLOAD_BEGIN=0x10, UPLOAD_CHUNK=0x11, UPLOAD_END=0x12, RUN=0x13,
           STOP=0x14, LIST=0x15, DELETE=0x16, SET_AUTORUN=0x17, TELEMETRY=0x20, TRACE=0x21, CALIBRATE=0x30)
ACK, INFO, PONG, SLOTS, NAK = 0x80, 0x81, 0x82, 0x85, 0x8F
TELEMETRY_DATA, TRACE_DATA, STATUS, ERROR = 0x90, 0x91, 0x92, 0x93
LOAD_ERRORS = {v: k for k, v in elxasm.SPEC["errors"]["load"].items()}
RUNTIME_ERRORS = {v: k for k, v in elxasm.SPEC["errors"]["runtime"].items()}
LINK_ERRORS = {0x41: "BAD_SLOT", 0x42: "BAD_SEQUENCE", 0x43: "TOO_LARGE", 0x44: "CRC_MISMATCH",
               0x45: "STORAGE", 0x46: "EMPTY_SLOT", 0x47: "UNKNOWN_CMD", 0x48: "BAD_ARGS"}


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(t, payload=b""):
    body = bytes([t]) + struct.pack("<H", len(payload)) + payload
    return b"\xA5\x5A" + body + struct.pack("<H", crc16(body))


class Parser:
    def __init__(self):
        self.buf = bytearray()

    def feed(self, data):
        self.buf += data
        out = []
        while True:
            i = self.buf.find(b"\xA5\x5A")
            if i < 0:
                self.buf = self.buf[-1:] if self.buf[-1:] == b"\xA5" else bytearray()
                return out
            del self.buf[:i]
            if len(self.buf) < 5:
                return out
            n = self.buf[3] | self.buf[4] << 8
            if n > 512:
                del self.buf[:2]
                continue
            if len(self.buf) < n + 7:
                return out
            body, crc = bytes(self.buf[2:5 + n]), self.buf[5 + n] | self.buf[6 + n] << 8
            if crc16(body) == crc:
                out.append((body[0], body[3:]))
                del self.buf[:n + 7]
            else:
                del self.buf[:2]


def err_name(code):
    return LOAD_ERRORS.get(code) or LINK_ERRORS.get(code) or f"0x{code:02x}"


class Robot:
    """Transport-agnostic protocol client."""

    def __init__(self, transport):
        self.t = transport
        self.parser = Parser()
        self.inbox = []
        self.mtu_payload = 180

    async def _pump(self, timeout):
        data = await self.t.read(timeout)
        for msg in self.parser.feed(data):
            if msg[0] == ERROR:
                e, script, pc = msg[1][0], msg[1][1], msg[1][2] | msg[1][3] << 8
                print(f"! runtime error {RUNTIME_ERRORS.get(e, e)} in script {script} at pc {pc}")
            elif msg[0] == STATUS:
                run, slot = msg[1][0], msg[1][1]
                print(f"~ status: {'running slot ' + str(slot) if run else 'idle'}")
            self.inbox.append(msg)

    async def request(self, cmd, payload=b"", expect=(ACK, NAK), timeout=3.0):
        self.inbox.clear()
        await self.t.write(frame(CMD[cmd], payload))
        end = time.time() + timeout
        while time.time() < end:
            await self._pump(0.05)
            for m in self.inbox:
                if m[0] in expect:
                    if m[0] == NAK:
                        raise RuntimeError(f"{cmd} rejected: {err_name(m[1][1])}")
                    return m
        raise TimeoutError(f"no reply to {cmd}")

    async def info(self):
        _, p = await self.request("HELLO", expect=(INFO,))
        caps = p[8:40]
        n_sys = sum(bin(b).count("1") for b in caps)
        name = p[44:64].split(b"\0")[0].decode()
        print(f"{name} firmware {p[0]}.{p[1]}  bytecode v{p[2]}  max program {p[3] | p[4] << 8} B")
        print(f"slots {p[5]}  scripts {p[6]}  vars {p[7]}  syscalls {n_sys}  leds {p[42]}  touch pads {p[43]}")
        return p

    async def list(self):
        _, p = await self.request("LIST", expect=(SLOTS,))
        for i in range(p[0]):
            e = p[1 + i * 20: 21 + i * 20]
            used, auto = e[1] & 1, e[1] & 2
            name = e[4:20].split(b"\0")[0].decode(errors="replace")
            print(f"  slot {e[0]}: " + (f"{name!r:20s} {e[2] | e[3] << 8:5d} B{'  (autorun)' if auto else ''}" if used else "-"))

    async def upload(self, image, slot):
        await self.request("UPLOAD_BEGIN", struct.pack("<BHI", slot, len(image), zlib.crc32(image) & 0xFFFFFFFF))
        off = 0
        while off < len(image):
            chunk = image[off:off + self.mtu_payload]
            _, p = await self.request("UPLOAD_CHUNK", struct.pack("<H", off) + chunk)
            off = p[1] | p[2] << 8  # robot tells us how much it has
        await self.request("UPLOAD_END", timeout=5)
        print(f"uploaded {len(image)} bytes to slot {slot}")

    async def watch(self, kind, seconds):
        if kind == "telemetry":
            await self.request("TELEMETRY", struct.pack("<H", 200))
        else:
            await self.request("TRACE", b"\x01")
        end = time.time() + seconds
        while time.time() < end:
            await self._pump(0.1)
            for t, p in self.inbox:
                if t == TELEMETRY_DATA:
                    dist, pitch, roll, yaw = struct.unpack_from("<Hhhh", p, 20)
                    print(f"line {list(p[4:12])} edge {list(p[12:16])} depth {list(p[16:20])} "
                          f"dist {dist / 10:.1f}cm pitch {pitch / 10:.0f} roll {roll / 10:.0f} yaw {yaw / 10:.0f} "
                          f"touch {p[28] & 1} snd {p[29]} joy {struct.unpack_from('<bb', p, 30)} btn {p[32]:03b} "
                          f"flags {p[34]:04b}" + (f" heading {(p[40] | p[41] << 8) / 10:.0f} magnet {p[42]}" if len(p) >= 44 else ""))
                elif t == TRACE_DATA:
                    nt = p[1]
                    th = [(p[2 + i * 3], p[3 + i * 3] | p[4 + i * 3] << 8) for i in range(nt)]
                    k = 2 + nt * 3
                    nv = p[k]
                    vs = struct.unpack_from(f"<{nv}f", p, k + 1) if nv else ()
                    print(f"threads {[('idle', 'run', 'wait')[s] + '@' + str(pc) for s, pc in th]} vars {[round(v, 2) for v in vs]}")
            self.inbox.clear()
        await self.request("TELEMETRY", struct.pack("<H", 0)) if kind == "telemetry" else await self.request("TRACE", b"\x00")


class SerialTransport:
    def __init__(self, port):
        import serial
        self.s = serial.Serial(port, 115200, timeout=0)
        time.sleep(0.2)
        self.s.reset_input_buffer()

    async def write(self, data):
        self.s.write(data)

    async def read(self, timeout):
        await asyncio.sleep(timeout)
        return self.s.read(4096)

    async def close(self):
        self.s.close()


class BleTransport:
    @classmethod
    async def connect(cls, name):
        from bleak import BleakClient, BleakScanner
        print("scanning...")
        dev = await BleakScanner.find_device_by_filter(
            lambda d, ad: (d.name or "").startswith(name or "Elxie") or SERVICE in (ad.service_uuids or []), timeout=8)
        if not dev:
            raise SystemExit("no Elxie robot found")
        self = cls()
        self.rx = bytearray()
        self.c = BleakClient(dev)
        await self.c.connect()
        await self.c.start_notify(TX, lambda _, d: self.rx.extend(d))
        print(f"connected to {dev.name}")
        return self

    async def write(self, data):
        for i in range(0, len(data), 180):
            await self.c.write_gatt_char(RX, data[i:i + 180], response=True)

    async def read(self, timeout):
        await asyncio.sleep(timeout)
        d, self.rx = bytes(self.rx), bytearray()
        return d

    async def close(self):
        await self.c.disconnect()


def load_image(path):
    if path.endswith(".elxa"):
        return elxasm.assemble(open(path, encoding="utf-8").read())[0]
    return open(path, "rb").read()


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--port", help="USB serial port")
    g.add_argument("--ble", nargs="?", const="", help="BLE name prefix (default Elxie)")
    ap.add_argument("command")
    ap.add_argument("arg", nargs="?")
    ap.add_argument("--slot", type=int, default=0)
    ap.add_argument("--run", action="store_true")
    ap.add_argument("--seconds", type=float, default=15)
    a = ap.parse_args()

    t = SerialTransport(a.port) if a.port else await BleTransport.connect(a.ble)
    r = Robot(t)
    try:
        c = a.command
        if c == "info":
            await r.info()
        elif c == "list":
            await r.list()
        elif c == "upload":
            await r.upload(load_image(a.arg), a.slot)
            if a.run:
                await r.request("RUN", bytes([a.slot]))
                print(f"running slot {a.slot}")
        elif c == "run":
            await r.request("RUN", bytes([int(a.arg)]))
        elif c == "stop":
            await r.request("STOP")
        elif c == "delete":
            await r.request("DELETE", bytes([int(a.arg)]))
        elif c == "autorun":
            await r.request("SET_AUTORUN", bytes([0xFF if a.arg == "none" else int(a.arg)]))
        elif c in ("telemetry", "trace"):
            await r.watch(c, a.seconds)
        elif c == "calibrate":
            if a.arg == "gyro":
                await r.request("CALIBRATE", b"\x03", timeout=5)
            elif a.arg == "compass":
                await r.request("CALIBRATE", b"\x04")
                print("robot is spinning for 6 s on its own - keep it on a flat floor, away from metal...")
                await asyncio.sleep(7)
            else:
                await r.request("CALIBRATE", b"\x01")
                print("slide the robot slowly over the line and the floor for 5 s...")
                await asyncio.sleep(5)
                await r.request("CALIBRATE", b"\x02")
            print("calibrated")
        else:
            raise SystemExit(f"unknown command {c}")
    except (RuntimeError, TimeoutError) as e:
        print(f"error: {e}")
        sys.exit(1)
    finally:
        await t.close()


if __name__ == "__main__":
    asyncio.run(main())
