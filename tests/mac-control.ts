// Synchronous -dbg-control client for macOS. sendMessage cannot await, and the
// async client already holds one connection, so window messages use a second
// socket. The server polls both.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { dlopen, FFIType, ptr, toArrayBuffer } from "bun:ffi";

export const TestLayout = 70;
const TestInvokeCommand = 87;
const TestInput = 121;
const TestDdeExecute = 129;
const kCopyDataDdeW = 0x44646557;

const WM_KILLFOCUS = 0x0008;
const WM_CLOSE = 0x0010;
const WM_KEYDOWN = 0x0100;
const WM_KEYUP = 0x0101;
const WM_CHAR = 0x0102;
const WM_COMMAND = 0x0111;
const WM_MOUSEMOVE = 0x0200;
const WM_LBUTTONDOWN = 0x0201;
const WM_LBUTTONUP = 0x0202;
const WM_LBUTTONDBLCLK = 0x0203;
const WM_RBUTTONDOWN = 0x0204;
const WM_RBUTTONUP = 0x0205;
const WM_RBUTTONDBLCLK = 0x0206;
const WM_MBUTTONDOWN = 0x0207;
const WM_MBUTTONUP = 0x0208;
const WM_MBUTTONDBLCLK = 0x0209;
const WM_MOUSEWHEEL = 0x020a;
const WM_MOUSEHWHEEL = 0x020e;

const MK_LBUTTON = 0x0001;
const MK_RBUTTON = 0x0002;
const MK_SHIFT = 0x0004;
const MK_CONTROL = 0x0008;
const MK_MBUTTON = 0x0010;

const AF_UNIX = 1;
const SOCK_STREAM = 1;
const SOL_SOCKET = 0xffff;
const SO_NOSIGPIPE = 0x1022;

type ControlArg = number | string | Uint8Array | ControlArg[];

const enum ArgType {
  End = 0,
  Int32 = 1,
  Bytes = 2,
  String = 3,
  List = 4,
}

let gPath = "";
let gFd = -1;
let gNextId = 1;
let cmdNames: Map<number, string> | null = null;

type Libc = ReturnType<typeof dlopen>;
let libc: Libc | null = null;

function lib(): Libc {
  if (!libc) {
    libc = dlopen("/usr/lib/libSystem.B.dylib", {
      socket: { args: [FFIType.i32, FFIType.i32, FFIType.i32], returns: FFIType.i32 },
      connect: { args: [FFIType.i32, FFIType.ptr, FFIType.u32], returns: FFIType.i32 },
      send: { args: [FFIType.i32, FFIType.ptr, FFIType.u64, FFIType.i32], returns: FFIType.i64 },
      recv: { args: [FFIType.i32, FFIType.ptr, FFIType.u64, FFIType.i32], returns: FFIType.i64 },
      close: { args: [FFIType.i32], returns: FFIType.i32 },
      setsockopt: { args: [FFIType.i32, FFIType.i32, FFIType.i32, FFIType.ptr, FFIType.u32], returns: FFIType.i32 },
      __error: { args: [], returns: FFIType.ptr },
    });
  }
  return libc;
}

function errno(): number {
  const p = lib().symbols.__error();
  if (!p) {
    return 0;
  }
  return new DataView(toArrayBuffer(p, 0, 4)).getInt32(0, true);
}

function closeFd(): void {
  if (gFd >= 0) {
    lib().symbols.close(gFd);
    gFd = -1;
  }
}

export function registerControlPath(path: string): void {
  if (path === gPath) {
    return;
  }
  closeFd();
  gPath = path;
}

function appendU16(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff);
}

function appendU32(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}

function encodeArg(out: number[], arg: ControlArg): void {
  if (typeof arg === "number") {
    appendU16(out, ArgType.Int32);
    appendU32(out, arg | 0);
    return;
  }
  if (typeof arg === "string") {
    const bytes = new TextEncoder().encode(arg);
    appendU16(out, ArgType.String);
    appendU32(out, bytes.length);
    for (const b of bytes) {
      out.push(b);
    }
    out.push(0);
    return;
  }
  if (Array.isArray(arg)) {
    appendU16(out, ArgType.List);
    appendU16(out, arg.length);
    for (const el of arg) {
      encodeArg(out, el);
    }
    return;
  }
  appendU16(out, ArgType.Bytes);
  appendU32(out, arg.byteLength);
  for (const b of arg) {
    out.push(b);
  }
}

function encodeRequest(cmd: number, id: number, args: ControlArg[]): Buffer {
  const payload: number[] = [];
  appendU16(payload, cmd);
  appendU16(payload, id);
  for (const arg of args) {
    encodeArg(payload, arg);
  }
  appendU16(payload, ArgType.End);
  const packet: number[] = [];
  appendU32(packet, payload.length);
  packet.push(...payload);
  return Buffer.from(packet);
}

class PacketReader {
  pos = 0;
  constructor(readonly data: Buffer) {}
  u16(): number {
    const v = this.data.readUInt16LE(this.pos);
    this.pos += 2;
    return v;
  }
  u32(): number {
    const v = this.data.readUInt32LE(this.pos);
    this.pos += 4;
    return v;
  }
  i32(): number {
    const v = this.data.readInt32LE(this.pos);
    this.pos += 4;
    return v;
  }
  bytes(len: number): Buffer {
    const v = this.data.subarray(this.pos, this.pos + len);
    this.pos += len;
    return v;
  }
}

function decodeArg(r: PacketReader): ControlArg | undefined {
  const type = r.u16();
  if (type === ArgType.End) {
    return undefined;
  }
  if (type === ArgType.Int32) {
    return r.i32();
  }
  if (type === ArgType.Bytes) {
    return r.bytes(r.u32());
  }
  if (type === ArgType.String) {
    const len = r.u32();
    const bytes = r.bytes(len);
    if (r.bytes(1)[0] !== 0) {
      throw new Error("invalid control string terminator");
    }
    return new TextDecoder().decode(bytes);
  }
  if (type === ArgType.List) {
    const n = r.u16();
    const list: ControlArg[] = [];
    for (let i = 0; i < n; i++) {
      const arg = decodeArg(r);
      if (arg === undefined) {
        throw new Error("unexpected end marker in control list");
      }
      list.push(arg);
    }
    return list;
  }
  throw new Error(`unknown control argument type ${type}`);
}

function connectUnix(path: string): number {
  const fd = lib().symbols.socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    throw new Error(`control socket() failed, errno=${errno()}`);
  }
  const one = new Int32Array([1]);
  lib().symbols.setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, ptr(one), 4);
  // sockaddr_un: sun_len, sun_family, sun_path[104]
  const addr = Buffer.alloc(106);
  const pathBytes = Buffer.from(path + "\0");
  const sockLen = 2 + pathBytes.length;
  if (sockLen > addr.length) {
    lib().symbols.close(fd);
    throw new Error(`control socket path is too long: ${path}`);
  }
  addr[0] = sockLen;
  addr[1] = AF_UNIX;
  pathBytes.copy(addr, 2);
  if (lib().symbols.connect(fd, ptr(addr), sockLen) !== 0) {
    const err = errno();
    lib().symbols.close(fd);
    throw new Error(`control connect ${path} failed, errno=${err}`);
  }
  return fd;
}

function ensureFd(): number {
  if (gFd >= 0) {
    return gFd;
  }
  if (!gPath) {
    throw new Error("no macOS control socket is connected");
  }
  const deadline = Date.now() + 2000;
  let last = "";
  while (Date.now() < deadline) {
    try {
      gFd = connectUnix(gPath);
      return gFd;
    } catch (e) {
      last = String((e as Error).message ?? e);
      Bun.sleepSync(20);
    }
  }
  throw new Error(last || "control connect failed");
}

function writeAll(fd: number, buf: Buffer): void {
  let off = 0;
  while (off < buf.length) {
    const n = Number(lib().symbols.send(fd, ptr(buf.subarray(off)), BigInt(buf.length - off), 0));
    if (n <= 0) {
      throw new Error(`control send failed, errno=${errno()}`);
    }
    off += n;
  }
}

function readExact(fd: number, len: number): Buffer {
  const out = Buffer.alloc(len);
  let got = 0;
  while (got < len) {
    const n = Number(lib().symbols.recv(fd, ptr(out.subarray(got)), BigInt(len - got), 0));
    if (n === 0) {
      throw new Error("control pipe closed while reading");
    }
    if (n < 0) {
      throw new Error(`control recv failed, errno=${errno()}`);
    }
    got += n;
  }
  return out;
}

export function macControlRequest(cmd: number, args: ControlArg[]): ControlArg[] {
  const id = gNextId++ & 0xffff;
  const packet = encodeRequest(cmd, id, args);
  let fd = ensureFd();
  try {
    writeAll(fd, packet);
  } catch (e) {
    closeFd();
    fd = ensureFd();
    writeAll(fd, packet);
    void e;
  }
  const sizeBuf = readExact(fd, 4);
  const size = sizeBuf.readUInt32LE(0);
  if (size < 2 || size > 16 * 1024 * 1024) {
    closeFd();
    throw new Error(`control response size ${size}`);
  }
  const payload = readExact(fd, size);
  const r = new PacketReader(payload);
  const responseId = r.u16();
  if (responseId !== id) {
    throw new Error(`control response id mismatch: got ${responseId}, expected ${id}`);
  }
  const result: ControlArg[] = [];
  for (;;) {
    const arg = decodeArg(r);
    if (arg === undefined) {
      break;
    }
    result.push(arg);
  }
  return result;
}

function cmdName(id: number): string {
  if (!cmdNames) {
    cmdNames = new Map();
    const src = readFileSync(join(import.meta.dir, "..", "src", "shared", "Commands.h"), "utf8");
    const re = /\b(Cmd\w+)\s*=\s*(\d+)\b/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(src)) !== null) {
      cmdNames.set(parseInt(m[2]!, 10), m[1]!);
    }
  }
  // A favorite or another custom command is not in Commands.h. The app
  // accepts the id as "#123".
  return cmdNames.get(id) ?? `#${id}`;
}

function sign16(v: bigint): number {
  const n = Number(v & 0xffffn);
  return n >= 0x8000 ? n - 0x10000 : n;
}

type Geom = { scale: number; ox: number; oy: number; cx: number; cy: number; cdx: number; cdy: number };

function geom(): Geom {
  const res = macControlRequest(TestLayout, []);
  const text = String(res[1] ?? "");
  const scale = Number(/scale=([0-9.]+)/.exec(text)?.[1] ?? "1") || 1;
  const canvas = /item name=canvas visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(text);
  const x = canvas ? Number(canvas[1]) : 0;
  const y = canvas ? Number(canvas[2]) : 0;
  const dx = canvas ? Number(canvas[3]) : 0;
  const dy = canvas ? Number(canvas[4]) : 0;
  return { scale, ox: x / scale, oy: y / scale, cx: x, cy: y, cdx: dx, cdy: dy };
}

// Frame-client dips to screen pixels. The origin is the gpui content view.
export function macClientToScreen(x: number, y: number): { x: number; y: number } {
  const res = macControlRequest(TestLayout, []);
  const text = String(res[1] ?? "");
  const c = /content origin=(-?\d+),(-?\d+)/.exec(text);
  if (!c) {
    return { x, y };
  }
  return { x: x + Number(c[1]), y: y + Number(c[2]) };
}

export function macFrameVisible(): boolean {
  try {
    const res = macControlRequest(TestLayout, []);
    const raw = String(res[1] ?? "");
    return res[0] === 0 && !raw.includes("no-window");
  } catch {
    return false;
  }
}

export function macFrameClientRect(): { left: number; top: number; right: number; bottom: number } {
  const res = macControlRequest(TestLayout, []);
  const text = String(res[1] ?? "");
  const scale = Number(/scale=([0-9.]+)/.exec(text)?.[1] ?? "1") || 1;
  const frame = /item name=frame visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(text);
  const dx = frame ? Number(frame[3]) : 0;
  const dy = frame ? Number(frame[4]) : 0;
  return { left: 0, top: 0, right: Math.round(dx / scale), bottom: Math.round(dy / scale) };
}

function dips(px: number, py: number, g: Geom): { x: number; y: number } {
  return { x: Math.round(px * g.scale), y: Math.round(py * g.scale) };
}

function modsOf(w: number): number {
  let m = 0;
  // Control-click is the context click. Command is orig's Ctrl.
  if (w & MK_CONTROL) {
    m |= 8;
  }
  if (w & MK_SHIFT) {
    m |= 2;
  }
  return m;
}

function buttonOf(msg: number): number {
  if (msg === WM_RBUTTONDOWN || msg === WM_RBUTTONUP || msg === WM_RBUTTONDBLCLK) {
    return 1;
  }
  if (msg === WM_MBUTTONDOWN || msg === WM_MBUTTONUP || msg === WM_MBUTTONDBLCLK) {
    return 2;
  }
  return 0;
}

function isMouseDown(msg: number): boolean {
  return (
    msg === WM_LBUTTONDOWN ||
    msg === WM_LBUTTONUP ||
    msg === WM_LBUTTONDBLCLK ||
    msg === WM_RBUTTONDOWN ||
    msg === WM_RBUTTONUP ||
    msg === WM_RBUTTONDBLCLK ||
    msg === WM_MBUTTONDOWN ||
    msg === WM_MBUTTONUP ||
    msg === WM_MBUTTONDBLCLK ||
    msg === WM_MOUSEMOVE
  );
}

// One click, not down then up: the run loop can snap in a real mouse move
// between the two and gpui treats that as a drag.
export function macClick(px: number, py: number, wParam = 0): void {
  const g = geom();
  const at = dips(px, py, g);
  input("click", at.x, at.y, 0, modsOf(wParam));
}

function input(kind: string, a: number, b: number, c: number, d: number): void {
  const res = macControlRequest(TestInput, [kind, a, b, c, d]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR")) {
    throw new Error(`TestInput ${kind} failed: ${raw}`);
  }
}

function invoke(name: string, x?: number, y?: number): void {
  const args: ControlArg[] = x === undefined ? [name] : [name, x, y];
  const res = macControlRequest(TestInvokeCommand, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR") || raw.startsWith("NOTREADY")) {
    throw new Error(`TestInvokeCommand ${name} failed: ${raw}`);
  }
}

// UTF-16 WM_COPYDATA 'DdeW' is a DDE command string. Returns the LRESULT.
export function macDdeExecute(dataId: number, text: string): bigint {
  if (dataId !== kCopyDataDdeW) {
    throw new Error(`macOS sendCopyDataW does not handle data id 0x${dataId.toString(16)}`);
  }
  const res = macControlRequest(TestDdeExecute, [text]);
  if (res[0] !== 0) {
    return 0n;
  }
  return String(res[1] ?? "") === "1" ? 1n : 0n;
}

// Frame-client pixels, as WM_COMMAND and WM_*BUTTON lParam carry on Windows.
// A command point is canvas pixels. TestInput wants frame dips.
export function macSendMessage(msg: number, wParam: number | bigint, lParam: number | bigint): bigint {
  const wp = BigInt(wParam);
  const lp = BigInt(lParam);
  if (msg === WM_COMMAND) {
    if (Number(wp >> 16n) !== 0) {
      return 0n;
    }
    const name = cmdName(Number(wp & 0xffffn));
    if (lp === 0n) {
      invoke(name);
      return 0n;
    }
    const g = geom();
    invoke(name, Math.round(sign16(lp) - g.ox), Math.round(sign16(lp >> 16n) - g.oy));
    return 0n;
  }
  if (msg === WM_CLOSE) {
    invoke("WM_CLOSE");
    return 0n;
  }
  // wParam 0 is the on-screen keyboard: no window took the focus.
  if (msg === WM_KILLFOCUS) {
    if (wp === 0n) {
      invoke("WM_KILLFOCUS");
    }
    return 0n;
  }
  if (msg === WM_KEYDOWN || msg === WM_KEYUP) {
    const repeat = msg === WM_KEYDOWN && (lp & (1n << 30n)) !== 0n ? 8 : 0;
    input(msg === WM_KEYDOWN ? "key" : "keyup", Number(wp & 0xffffn), repeat, 0, 0);
    return 0n;
  }
  if (msg === WM_CHAR) {
    input("char", Number(wp & 0xffffn), 0, 0, 0);
    return 0n;
  }
  if (msg === WM_MOUSEWHEEL || msg === WM_MOUSEHWHEEL) {
    const g = geom();
    let px = sign16(lp);
    let py = sign16(lp >> 16n);
    // A posted wheel of lParam 0 is "the document", not the corner pixel.
    if (lp === 0n && g.cdx > 0 && g.cdy > 0) {
      px = Math.round(g.ox + g.cdx / g.scale / 2);
      py = Math.round(g.oy + g.cdy / g.scale / 2);
    }
    const at = dips(px, py, g);
    let delta = sign16(wp >> 16n);
    if (msg === WM_MOUSEHWHEEL) {
      delta += 1000000;
    }
    input("wheel", at.x, at.y, delta, modsOf(Number(wp & 0xffffn)));
    return 0n;
  }
  if (isMouseDown(msg)) {
    const g = geom();
    const at = dips(sign16(lp), sign16(lp >> 16n), g);
    const w = Number(wp & 0xffffn);
    const m = modsOf(w);
    if (msg === WM_MOUSEMOVE) {
      let c = 0;
      if (w & MK_LBUTTON) {
        c = 1;
      } else if (w & MK_RBUTTON) {
        c = 2;
      } else if (w & MK_MBUTTON) {
        c = 3;
      }
      input("move", at.x, at.y, c, m);
      return 0n;
    }
    const down =
      msg === WM_LBUTTONDOWN ||
      msg === WM_RBUTTONDOWN ||
      msg === WM_MBUTTONDOWN ||
      msg === WM_LBUTTONDBLCLK ||
      msg === WM_RBUTTONDBLCLK ||
      msg === WM_MBUTTONDBLCLK;
    input(down ? "down" : "up", at.x, at.y, buttonOf(msg), m);
    return 0n;
  }
  throw new Error(`macOS sendMessage does not handle msg 0x${msg.toString(16)}`);
}

export function macSendText(text: string): void {
  const res = macControlRequest(TestInput, ["text", text]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`set text failed: ${raw}`);
  }
}
