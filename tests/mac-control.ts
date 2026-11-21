// Synchronous -dbg-control client for macOS. sendMessage cannot await, and the
// async client already holds one connection, so window messages use a second
// socket. The server polls both.

import { mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { inflateSync } from "node:zlib";
import { dlopen, FFIType, ptr, toArrayBuffer } from "bun:ffi";
import { IS_WIN } from "./host.ts";

export const TestLayout = 70;
const TestInvokeCommand = 87;
const TestInput = 121;
const TestDdeExecute = 129;
const TestCaptureWindow = 170;
const TestWindows = 172;
const TestToolWindow = 125;
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
const WM_XBUTTONDOWN = 0x020b;
const WM_XBUTTONUP = 0x020c;
const WM_NULL = 0x0000;
const WM_SYSCOMMAND = 0x0112;
const SC_CLOSE = 0xf060;
const SC_MINIMIZE = 0xf020;
const SC_RESTORE = 0xf120;
const WM_HSCROLL = 0x0114;
const WM_VSCROLL = 0x0115;
const WM_MOUSEWHEEL = 0x020a;
const WM_MOUSEHWHEEL = 0x020e;

const MK_LBUTTON = 0x0001;
const MK_RBUTTON = 0x0002;
const MK_SHIFT = 0x0004;
const MK_CONTROL = 0x0008;
const MK_ALT = 0x0020;
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
    if (IS_WIN) {
      kernel32().symbols.CloseHandle(gFd);
    } else {
      lib().symbols.close(gFd);
    }
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

// Windows: a second, synchronous connection to the named pipe (the server
// serves one thread per client)
const GENERIC_READ_WRITE = 0xc0000000;
const OPEN_EXISTING = 3;

let k32: ReturnType<typeof dlopen> | null = null;

function kernel32(): ReturnType<typeof dlopen> {
  if (!k32) {
    k32 = dlopen("kernel32.dll", {
      CreateFileW: {
        args: [FFIType.ptr, FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.u32, FFIType.u32, FFIType.ptr],
        returns: FFIType.i64_fast,
      },
      WriteFile: {
        args: [FFIType.i64_fast, FFIType.ptr, FFIType.u32, FFIType.ptr, FFIType.ptr],
        returns: FFIType.i32,
      },
      ReadFile: {
        args: [FFIType.i64_fast, FFIType.ptr, FFIType.u32, FFIType.ptr, FFIType.ptr],
        returns: FFIType.i32,
      },
      PeekNamedPipe: {
        args: [FFIType.i64_fast, FFIType.ptr, FFIType.u32, FFIType.ptr, FFIType.ptr, FFIType.ptr],
        returns: FFIType.i32,
      },
      CloseHandle: { args: [FFIType.i64_fast], returns: FFIType.i32 },
      GetLastError: { args: [], returns: FFIType.u32 },
    });
  }
  return k32;
}

function connectPipe(path: string): number {
  const name = Buffer.from(path + String.fromCharCode(0), "utf16le");
  const h = Number(kernel32().symbols.CreateFileW(ptr(name), GENERIC_READ_WRITE, 0, null, OPEN_EXISTING, 0, null));
  if (h === -1) {
    throw new Error(`control CreateFileW ${path} failed, err=${kernel32().symbols.GetLastError()}`);
  }
  return h;
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
      gFd = IS_WIN ? connectPipe(gPath) : connectUnix(gPath);
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
    if (IS_WIN) {
      const part = buf.subarray(off);
      const done = new Uint32Array(1);
      if (!kernel32().symbols.WriteFile(fd, ptr(part), part.length, ptr(done), null) || done[0] === 0) {
        throw new Error(`control WriteFile failed, err=${kernel32().symbols.GetLastError()}`);
      }
      off += done[0]!;
      continue;
    }
    const n = Number(lib().symbols.send(fd, ptr(buf.subarray(off)), BigInt(buf.length - off), 0));
    if (n <= 0) {
      throw new Error(`control send failed, errno=${errno()}`);
    }
    off += n;
  }
}

// A synchronous ReadFile has no timeout: input that opens a modal native
// window (MessageBox, Save As) never gets its reply and would hang the test.
const kPipeReplyTimeoutMs = 60_000;

// waits until the pipe has data to read
function waitPipeData(fd: number, deadline: number): void {
  const avail = new Uint32Array(1);
  const start = Date.now();
  for (;;) {
    if (!kernel32().symbols.PeekNamedPipe(fd, null, 0, null, ptr(avail), null)) {
      throw new Error("control pipe closed while reading");
    }
    if (avail[0]! > 0) {
      return;
    }
    if (Date.now() >= deadline) {
      // the late reply would answer the next request
      closeFd();
      throw new Error(
        `no control reply in ${kPipeReplyTimeoutMs / 1000}s: the app is likely in a modal native window (MessageBox, file dialog)`,
      );
    }
    // replies usually take a few ms: spin briefly before sleeping
    if (Date.now() - start > 20) {
      Bun.sleepSync(1);
    }
  }
}

function readExact(fd: number, len: number): Buffer {
  const out = Buffer.alloc(len);
  let got = 0;
  const deadline = Date.now() + kPipeReplyTimeoutMs;
  while (got < len) {
    if (IS_WIN) {
      waitPipeData(fd, deadline);
      const part = out.subarray(got);
      const done = new Uint32Array(1);
      if (!kernel32().symbols.ReadFile(fd, ptr(part), part.length, ptr(done), null) || done[0] === 0) {
        throw new Error("control pipe closed while reading");
      }
      got += done[0]!;
      continue;
    }
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
      const id = parseInt(m[2]!, 10);
      // CmdLast shares CmdMergePDF's number. Keep the real command name.
      if (!cmdNames.has(id)) {
        cmdNames.set(id, m[1]!);
      }
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

// What EnumWindows and GetWindowText & co. tell a Windows test, for each frame
// and tool window. hwnd is the app's id for the window: 1 is the first frame,
// the one a test starts with. toolName: a tool window's TestToolWindow name.
// Empty when no app is connected.
export type MacWindow = {
  hwnd: number;
  className: string;
  pid: number;
  visible: boolean;
  rect: { left: number; top: number; right: number; bottom: number };
  title: string;
  // a tool window's frame
  owner: number;
  toolName?: string;
};

export function macWindows(): MacWindow[] {
  let raw = "";
  try {
    raw = String(macControlRequest(TestWindows, [])[1] ?? "");
  } catch {
    return [];
  }
  const out: MacWindow[] = [];
  for (const line of raw.split("\n")) {
    const m =
      /^hwnd=(\d+) class=(\S*) pid=(\d+) visible=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) (?:owner=(\d+) name=(\S+) )?title=(.*)$/.exec(
        line,
      );
    if (!m) {
      continue;
    }
    const [x, y, dx, dy] = [+m[5]!, +m[6]!, +m[7]!, +m[8]!];
    out.push({
      hwnd: +m[1]!,
      className: m[2]!,
      pid: +m[3]!,
      visible: m[4] === "1",
      rect: { left: x, top: y, right: x + dx, bottom: y + dy },
      title: m[11]!,
      owner: m[9] ? +m[9] : 0,
      toolName: m[10],
    });
  }
  return out;
}

export function macWindow(hwnd: number): MacWindow | undefined {
  return macWindows().find((w) => w.hwnd === hwnd);
}

// Runs fn with the app's requests aimed at frame hwnd. 1 (and 0) is the first
// frame, which they go to anyway.
export function macWithTarget<T>(hwnd: number, fn: () => T): T {
  if (hwnd <= 1) {
    return fn();
  }
  macControlRequest(TestWindows, ["target", hwnd]);
  try {
    return fn();
  } finally {
    macControlRequest(TestWindows, ["target", 0]);
  }
}

// A message to a tool window: TestToolWindow input in its client dips, which
// are a Windows test's client pixels at 100%. A dialog's IDOK / IDCANCEL is
// Enter / Esc.
export function macToolWindowMessage(
  name: string,
  msg: number,
  wParam: number | bigint,
  lParam: number | bigint,
): bigint {
  const wp = BigInt(wParam);
  const lp = BigInt(lParam);
  const tool = (args: ControlArg[]): string => {
    const res = macControlRequest(TestToolWindow, args);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0 || raw.startsWith("ERR")) {
      throw new Error(`TestToolWindow ${args.join(" ")} failed: ${raw}`);
    }
    return raw;
  };
  const input = (kind: string, a: number, b = 0, c = 0, d = 0) => tool(["input", name, kind, a, b, c, d]);
  if (msg === WM_CLOSE || (msg === WM_SYSCOMMAND && (wp & 0xfff0n) === BigInt(SC_CLOSE))) {
    tool(["close", name]);
    return 0n;
  }
  if (msg === WM_COMMAND && (wp & 0xffffn) <= 2n) {
    input("key", (wp & 0xffffn) === 1n ? 0x0d : 0x1b);
    return 0n;
  }
  if (msg === WM_KEYDOWN) {
    const vk = Number(wp & 0xffffn);
    const raw = input("key", vk);
    // TranslateMessage: the key's character, unless the key was handled
    const ch = keyChar(vk);
    if (ch && !raw.includes("eaten=1")) {
      input("char", ch);
    }
    return raw.includes("eaten=1") ? 1n : 0n;
  }
  if (msg === WM_KEYUP) {
    input("keyup", Number(wp & 0xffffn));
    return 0n;
  }
  if (msg === WM_CHAR) {
    input("char", Number(wp & 0xffffn));
    return 0n;
  }
  if (isMouseDown(msg)) {
    const x = sign16(lp);
    const y = sign16(lp >> 16n);
    const w = Number(wp & 0xffffn);
    const m = modsOf(w, { hwnd: 0, screenToClient: (a, b) => ({ x: a, y: b }) });
    if (msg === WM_MOUSEMOVE) {
      input("move", x, y, w & MK_LBUTTON ? 1 : w & MK_RBUTTON ? 2 : w & MK_MBUTTON ? 3 : 0, m);
      return 0n;
    }
    const down =
      msg === WM_LBUTTONDOWN ||
      msg === WM_RBUTTONDOWN ||
      msg === WM_MBUTTONDOWN ||
      msg === WM_LBUTTONDBLCLK ||
      msg === WM_RBUTTONDBLCLK ||
      msg === WM_MBUTTONDBLCLK;
    input(down ? "down" : "up", x, y, buttonOf(msg), m);
    return 0n;
  }
  if (msg === WM_NULL) {
    return 0n;
  }
  throw new Error(`macOS: tool window ${name} does not take msg 0x${msg.toString(16)}`);
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

// RGBA rows of an 8-bit gray, RGB or RGBA (with or without alpha),
// non-interlaced PNG (what AppKit and mupdf write)
function decodePng(png: Buffer): { w: number; h: number; rgba: Uint8Array } {
  let pos = 8;
  let w = 0;
  let h = 0;
  let channels = 0;
  const idat: Buffer[] = [];
  while (pos + 8 <= png.length) {
    const n = png.readUInt32BE(pos);
    const type = png.toString("latin1", pos + 4, pos + 8);
    const data = png.subarray(pos + 8, pos + 8 + n);
    if (type === "IHDR") {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      const depth = data[8];
      const colorType = data[9];
      channels = ({ 0: 1, 2: 3, 4: 2, 6: 4 } as Record<number, number>)[colorType!] ?? 0;
      if (depth !== 8 || !channels || data[12] !== 0) {
        throw new Error(`decodePng: unsupported PNG (depth ${depth}, color type ${colorType}, interlace ${data[12]})`);
      }
    } else if (type === "IDAT") {
      idat.push(data);
    }
    pos += 12 + n;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = w * channels;
  const out = new Uint8Array(w * h * 4);
  let prev = new Uint8Array(stride);
  for (let y = 0; y < h; y++) {
    const filter = raw[y * (stride + 1)];
    const line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    const cur = new Uint8Array(stride);
    for (let i = 0; i < stride; i++) {
      const a = i >= channels ? cur[i - channels]! : 0;
      const b = prev[i]!;
      const c = i >= channels ? prev[i - channels]! : 0;
      let v = line[i]!;
      if (filter === 1) {
        v += a;
      } else if (filter === 2) {
        v += b;
      } else if (filter === 3) {
        v += (a + b) >> 1;
      } else if (filter === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a);
        const pb = Math.abs(p - b);
        const pc = Math.abs(p - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      cur[i] = v & 0xff;
    }
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4;
      const gray = channels < 3;
      out[o] = cur[x * channels]!;
      out[o + 1] = cur[x * channels + (gray ? 0 : 1)]!;
      out[o + 2] = cur[x * channels + (gray ? 0 : 2)]!;
      out[o + 3] = channels === 4 || channels === 2 ? cur[x * channels + channels - 1]! : 255;
    }
    prev = cur;
  }
  return { w, h, rgba: out };
}

// getClipboardDibInfo on macOS: the pasteboard's image as a pasting app gets
// it, PNG or else TIFF (re-encoded as PNG by AppKit). null when it has none.
export function macClipboardImageInfo(): { width: number; height: number; bitCount: number; nonBlack: number } | null {
  const dir = join(import.meta.dir, "..", ".work", "tests-tmp", "tmp");
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `clipboard-${process.pid}.png`);
  rmSync(path, { force: true });
  const script = `ObjC.import("AppKit");
const pb = $.NSPasteboard.generalPasteboard;
let d = pb.dataForType($.NSPasteboardTypePNG);
if (d.isNil()) {
  const t = pb.dataForType($.NSPasteboardTypeTIFF);
  if (!t.isNil()) d = $.NSBitmapImageRep.imageRepWithData(t).representationUsingTypeProperties($.NSBitmapImageFileTypePNG, $());
}
d.isNil() ? "none" : d.writeToFileAtomically(${JSON.stringify(path)}, true) ? "ok" : "fail";`;
  const r = Bun.spawnSync(["osascript", "-l", "JavaScript", "-e", script], { stdout: "pipe", stderr: "pipe" });
  if (r.stdout.toString().trim() !== "ok") {
    return null;
  }
  const png = decodePng(readFileSync(path));
  rmSync(path, { force: true });
  let nonBlack = 0;
  for (let i = 0; i < png.w * png.h; i++) {
    if ((png.rgba[i * 4]! | png.rgba[i * 4 + 1]! | png.rgba[i * 4 + 2]!) !== 0) {
      nonBlack++;
    }
  }
  return { width: png.w, height: png.h, bitCount: 32, nonBlack };
}

// captureWindowPixels on macOS: the first window as drawn, as BGRA (4 bytes a
// pixel, top-down) in client coordinates, which are points: the capture is in
// device pixels, sampled at each point's center
export function macCaptureClientPixels(): { w: number; h: number; data: Uint8Array } | null {
  const rc = macFrameClientRect();
  const w = rc.right - rc.left;
  const h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) {
    return null;
  }
  // util.ts's tmpPath() dir: util.ts imports this file
  const dir = join(import.meta.dir, "..", ".work", "tests-tmp", "tmp");
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `capture-${process.pid}.png`);
  const res = macControlRequest(TestCaptureWindow, [path]);
  if (res[0] !== 0) {
    return null;
  }
  // with web views in the window the PNG is written once their snapshots
  // come back, after the request has answered
  let png: ReturnType<typeof decodePng> | null = null;
  const deadline = Date.now() + 5000;
  while (!png) {
    try {
      png = decodePng(readFileSync(path));
    } catch (e) {
      if (Date.now() > deadline) {
        throw e;
      }
      Bun.sleepSync(20);
    }
  }
  rmSync(path, { force: true });
  const sx = png.w / w;
  const sy = png.h / h;
  const data = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) {
    const py = Math.min(png.h - 1, Math.floor((y + 0.5) * sy));
    for (let x = 0; x < w; x++) {
      const px = Math.min(png.w - 1, Math.floor((x + 0.5) * sx));
      const i = (py * png.w + px) * 4;
      const o = (y * w + x) * 4;
      data[o] = png.rgba[i + 2]!;
      data[o + 1] = png.rgba[i + 1]!;
      data[o + 2] = png.rgba[i]!;
      data[o + 3] = png.rgba[i + 3]!;
    }
  }
  return { w, h, data };
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

// Windows: the gpui window a message was aimed at. TestInput takes its client
// pixels as they are and scales them itself. clientCenter: a tool window's,
// where a wheel aimed at the window (lParam 0) goes.
export type InputTarget = {
  hwnd: number;
  screenToClient: (x: number, y: number) => { x: number; y: number };
  clientCenter?: () => { x: number; y: number };
};

function modsOf(w: number, target?: InputTarget): number {
  let m = 0;
  // Control-click is the context click. Command is orig's Ctrl. Alt is bit 4.
  if (w & MK_CONTROL) {
    m |= target ? 1 : 8;
  }
  if (w & MK_SHIFT) {
    m |= 2;
  }
  if (w & MK_ALT) {
    m |= 4;
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
export function macClick(px: number, py: number, wParam = 0, target?: InputTarget): void {
  const at = target ? { x: px, y: py } : dips(px, py, geom());
  input("click", at.x, at.y, 0, modsOf(wParam, target), target);
}

function input(kind: string, a: number, b: number, c: number, d: number, target?: InputTarget): string {
  const args: ControlArg[] = [kind, a, b, c, d];
  if (target) {
    args.push(target.hwnd);
  }
  const res = macControlRequest(TestInput, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR")) {
    throw new Error(`TestInput ${kind} failed: ${raw}`);
  }
  return raw;
}

// the character TranslateMessage makes of an unshifted key, 0 for none
function keyChar(vk: number): number {
  if (vk >= 0x41 && vk <= 0x5a) {
    return vk + 0x20;
  }
  if ((vk >= 0x30 && vk <= 0x39) || vk === 0x20) {
    return vk;
  }
  return 0;
}

// A posted WM_KEYDOWN: the message loop's TranslateMessage types the key's
// character unless the app handled the key, as ngInput does on Windows.
export function macPostKeyDown(wParam: number | bigint, lParam: number | bigint): void {
  const vk = Number(BigInt(wParam) & 0xffffn);
  const repeat = (BigInt(lParam) & (1n << 30n)) !== 0n ? 8 : 0;
  const raw = input("key", vk, repeat, 0, 0);
  const ch = keyChar(vk);
  if (ch && !raw.includes("eaten=1")) {
    input("char", ch, 0, 0, 0);
  }
}

// a key with Shift (VK_SHIFT), Ctrl (VK_CONTROL) or Alt (VK_MENU) held: TestInput
// takes the modifiers with the key, where Windows reads them from the key state
export function macPostKeyWithModifier(vk: number, modVk: number): void {
  const mods = modVk === 0x10 ? 2 : modVk === 0x11 ? 1 : modVk === 0x12 ? 4 : 0;
  input("key", vk, mods, 0, 0);
}

// ShowWindow on macOS: minimize, or bring the minimized frame back
export function macShowWindow(minimize: boolean): void {
  invoke(minimize ? "SC_MINIMIZE" : "SC_RESTORE");
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
// A command point is canvas pixels. TestInput wants frame dips, or with a
// target (Windows) that window's client pixels.
export function macSendMessage(
  msg: number,
  wParam: number | bigint,
  lParam: number | bigint,
  target?: InputTarget,
): bigint {
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
  // The caption close button. DefWindowProc turns it into WM_CLOSE.
  if (msg === WM_SYSCOMMAND) {
    const sc = wp & 0xfff0n;
    if (sc === BigInt(SC_CLOSE)) {
      invoke("WM_CLOSE");
    } else if (sc === BigInt(SC_MINIMIZE)) {
      invoke("SC_MINIMIZE");
    } else if (sc === BigInt(SC_RESTORE)) {
      invoke("SC_RESTORE");
    }
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
    const raw = input(msg === WM_KEYDOWN ? "key" : "keyup", Number(wp & 0xffffn), repeat, 0, 0, target);
    // with a target: 1 for a key the app handled
    return target && raw.includes("eaten=1") ? 1n : 0n;
  }
  if (msg === WM_CHAR) {
    input("char", Number(wp & 0xffffn), 0, 0, 0, target);
    return 0n;
  }
  if (msg === WM_HSCROLL || msg === WM_VSCROLL) {
    invoke(msg === WM_VSCROLL ? "WM_VSCROLL" : "WM_HSCROLL", Number(wp & 0xffffn), Number((wp >> 16n) & 0xffffn));
    return 0n;
  }
  if (msg === WM_MOUSEWHEEL || msg === WM_MOUSEHWHEEL) {
    const g = geom();
    // a wheel's lParam is in screen coordinates, unlike the other mouse messages
    const client = target
      ? target.screenToClient(sign16(lp), sign16(lp >> 16n))
      : (() => {
          const origin = macClientToScreen(0, 0);
          return { x: sign16(lp) - origin.x, y: sign16(lp >> 16n) - origin.y };
        })();
    let px = client.x;
    let py = client.y;
    // A posted wheel of lParam 0 is "the document", not the corner pixel.
    if (lp === 0n && target?.clientCenter) {
      ({ x: px, y: py } = target.clientCenter());
    } else if (lp === 0n && g.cdx > 0 && g.cdy > 0) {
      px = Math.round(g.ox + g.cdx / g.scale / 2);
      py = Math.round(g.oy + g.cdy / g.scale / 2);
    }
    const at = target ? { x: px, y: py } : dips(px, py, g);
    let delta = sign16(wp >> 16n);
    if (msg === WM_MOUSEHWHEEL) {
      delta += 1000000;
    }
    input("wheel", at.x, at.y, delta, modsOf(Number(wp & 0xffffn), target), target);
    return 0n;
  }
  if (isMouseDown(msg)) {
    const px = sign16(lp);
    const py = sign16(lp >> 16n);
    const at = target ? { x: px, y: py } : dips(px, py, geom());
    const w = Number(wp & 0xffffn);
    const m = modsOf(w, target);
    if (msg === WM_MOUSEMOVE) {
      let c = 0;
      if (w & MK_LBUTTON) {
        c = 1;
      } else if (w & MK_RBUTTON) {
        c = 2;
      } else if (w & MK_MBUTTON) {
        c = 3;
      }
      input("move", at.x, at.y, c, m, target);
      return 0n;
    }
    const down =
      msg === WM_LBUTTONDOWN ||
      msg === WM_RBUTTONDOWN ||
      msg === WM_MBUTTONDOWN ||
      msg === WM_LBUTTONDBLCLK ||
      msg === WM_RBUTTONDBLCLK ||
      msg === WM_MBUTTONDBLCLK;
    input(down ? "down" : "up", at.x, at.y, buttonOf(msg), m, target);
    return 0n;
  }
  // a side button: XBUTTON1 / XBUTTON2 in the high word, TestInput's buttons 3 / 4
  if (msg === WM_XBUTTONDOWN || msg === WM_XBUTTONUP) {
    const px = sign16(lp);
    const py = sign16(lp >> 16n);
    const at = target ? { x: px, y: py } : dips(px, py, geom());
    const button = Number((wp >> 16n) & 0xffffn) === 2 ? 4 : 3;
    input(msg === WM_XBUTTONDOWN ? "down" : "up", at.x, at.y, button, modsOf(Number(wp & 0xffffn), target), target);
    return 0n;
  }
  // Windows answers it once the window has handled what it was sent before
  if (msg === WM_NULL) {
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
