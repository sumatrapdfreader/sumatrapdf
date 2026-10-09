// Command-line client for the app's automation channel (`-dbg-control <pipe>`,
// src/SumatraControl.cpp). Orig drives the same protocol from tests/control.ts;
// this is the small half of it: launch (or attach to) SumatraPDF, send one
// command, print the reply.
//
//   bun cmd/ng-dbg-control.ts [-exe <path>] [-pipe <name>] [--] <cmd> [args...]
//   bun cmd/ng-dbg-control.ts TestCurrentTab docs/test/zlib.3.pdf
//
// <cmd> is a ControlCommand name or number; an argument that parses as an
// integer is sent as an int, everything else as a string. When the command
// line names a document (any argument before the command that is a path), the
// app is started with it and quit again afterwards.

import { createConnection, type Socket } from "node:net";
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";

/** Same numbering as ControlCmd in src/SumatraControl.cpp. */
export const controlCommands: Record<string, number> = {
  Ping: 1,
  Quit: 2,
  TestSelectionTranslate: 15,
  TestFileKind: 25,
  TestFavoriteNav: 34,
  TestKeyboardLinkFollow: 36,
  TestSelectTextKeyboard: 40,
  TestAIChat: 41,
  TestAIChatReplay: 42,
  TestDisplayMode: 49,
  TestDocumentFontList: 53,
  WaitRenderIdle: 54,
  SetNotificationsEnabled: 55,
  TestGetPolicies: 62,
  TestPageBoxes: 63,
  TestDocumentSignatures: 64,
  TestFindHistory: 66,
  TestConvertToImages: 69,
  TestFindWindowContents: 77,
  TestLayout: 70,
  TestFindUiState: 78,
  TestReadAloudPlaybackBar: 80,
  TestReadingAutoScroll: 97,
  TestLinkDestHighlight: 68,
  TestConvertToPdf: 86,
  TestInvokeCommand: 87,
  TestPerfStats: 101,
  TestOverlayState: 102,
  WaitSessionRestored: 103,
  TestNavFiles: 104,
  TestMergePdf: 115,
  TestCurrentTab: 88,
  TestCommandVisibility: 89,
  TestExtractPages: 90,
  TestAnnotFilter: 91,
  CrashMe: 93,
  TestDocumentProperties: 94,
  TestSaveSelectionAsImage: 96,
  TestMainMenu: 117,
  TestDefaultAppNotif: 118,
  StartPerfLog: 119,
  StopPerfLog: 120,
  TestInput: 121,
  TestUiState: 122,
  TestOleDragDrop: 123,
  TestNativeFileDlg: 124,
  TestToolWindow: 125,
  TestNativeMsgBox: 126,
  TestContextMenuAt: 127,
  TestSavePathDialog: 128,
};

const argTypeEnd = 0;
const argTypeInt32 = 1;
const argTypeBytes = 2;
const argTypeString = 3;

type Arg = number | string;

function appendU16(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff);
}

function appendU32(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}

function encodeArg(out: number[], arg: Arg): void {
  if (typeof arg === "number") {
    appendU16(out, argTypeInt32);
    appendU32(out, arg | 0);
    return;
  }
  const bytes = new TextEncoder().encode(arg);
  appendU16(out, argTypeString);
  appendU32(out, bytes.length);
  for (const b of bytes) out.push(b);
  out.push(0);
}

function encodeRequest(cmd: number, id: number, args: Arg[]): Buffer {
  const payload: number[] = [];
  appendU16(payload, cmd);
  appendU16(payload, id);
  for (const a of args) encodeArg(payload, a);
  appendU16(payload, argTypeEnd);
  const packet: number[] = [];
  appendU32(packet, payload.length);
  return Buffer.from(packet.concat(payload));
}

function decodeArgs(data: Buffer): Arg[] {
  const out: Arg[] = [];
  let pos = 2; // reqId
  for (;;) {
    if (pos + 2 > data.length) break;
    const type = data.readUInt16LE(pos);
    pos += 2;
    if (type === argTypeEnd) break;
    if (type === argTypeInt32) {
      out.push(data.readInt32LE(pos));
      pos += 4;
      continue;
    }
    if (type === argTypeString || type === argTypeBytes) {
      const n = data.readUInt32LE(pos);
      pos += 4;
      out.push(new TextDecoder().decode(data.subarray(pos, pos + n)));
      pos += n + (type === argTypeString ? 1 : 0);
      continue;
    }
    throw new Error(`unknown control argument type ${type}`);
  }
  return out;
}

// A named pipe on Windows; elsewhere a unix domain socket, <name> being its
// path or, without a "/", /tmp/<name>.sock (see SumatraControl.cpp).
function pipePath(name: string): string {
  if (process.platform !== "win32") {
    return name.includes("/") ? name : `/tmp/${name}.sock`;
  }
  return name.startsWith("\\\\.\\pipe\\") ? name : `\\\\.\\pipe\\${name}`;
}

function defaultExe(): string {
  if (process.platform === "win32") return "out/win/dbg/SumatraPDF.exe";
  return process.platform === "darwin" ? "out/mac/dbg/SumatraPDF" : "out/linux/dbg/SumatraPDF";
}

export async function connect(name: string, timeoutMs: number): Promise<Socket> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    try {
      return await new Promise<Socket>((resolve, reject) => {
        const s = createConnection(pipePath(name));
        s.once("connect", () => resolve(s));
        s.once("error", reject);
      });
    } catch (err) {
      if (Date.now() >= deadline) throw err;
      await new Promise((r) => setTimeout(r, 100));
    }
  }
}

async function readExactly(s: Socket, n: number): Promise<Buffer> {
  const chunks: Buffer[] = [];
  let total = 0;
  while (total < n) {
    const chunk = s.read(n - total) as Buffer | null;
    if (chunk) {
      chunks.push(chunk);
      total += chunk.length;
      continue;
    }
    await new Promise<void>((resolve, reject) => {
      const onReadable = () => {
        s.off("error", onError);
        resolve();
      };
      const onError = (e: Error) => {
        s.off("readable", onReadable);
        reject(e);
      };
      s.once("readable", onReadable);
      s.once("error", onError);
    });
  }
  return Buffer.concat(chunks);
}

export async function sendCommand(s: Socket, cmd: number, args: Arg[]): Promise<Arg[]> {
  s.write(encodeRequest(cmd, 1, args));
  const size = (await readExactly(s, 4)).readUInt32LE(0);
  return decodeArgs(await readExactly(s, size));
}

function usage(): never {
  const names = Object.keys(controlCommands).join(", ");
  console.log("usage: bun cmd/ng-dbg-control.ts [-exe <path>] [-pipe <name>] [<file>] <cmd> [args...]");
  console.log(`commands: ${names}`);
  process.exit(1);
}

async function main(): Promise<void> {
  const argv = process.argv.slice(2);
  let exe = defaultExe();
  let pipe = "";
  let attach = false;
  const rest: string[] = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]!;
    if (a === "-exe") exe = argv[++i]!;
    else if (a === "-pipe") {
      pipe = argv[++i]!;
      attach = true;
    } else rest.push(a);
  }
  const cmdIdx = rest.findIndex((a) => controlCommands[a] !== undefined || /^[1-9]\d*$/.test(a));
  if (cmdIdx < 0) usage();
  const files = rest.slice(0, cmdIdx).filter((a) => existsSync(a));
  const cmdArgs = rest.slice(cmdIdx);
  const cmdName = cmdArgs.shift()!;
  const cmd = controlCommands[cmdName] ?? Number(cmdName);
  if (!Number.isFinite(cmd) || cmd <= 0) usage();
  const args: Arg[] = cmdArgs.map((a) => (/^-?\d+$/.test(a) ? Number(a) : a));

  if (!pipe) pipe = `sumatra-ctl-${process.pid}-${Date.now()}`;
  let child: ReturnType<typeof spawn> | undefined;
  if (!attach) {
    child = spawn(exe, ["-for-testing", ...files, "-dbg-control", pipe], { stdio: "ignore", detached: false });
  }
  const s = await connect(pipe, 15000);
  try {
    const res = await sendCommand(s, cmd, args);
    for (const r of res) console.log(typeof r === "number" ? `exitCode=${r}` : r);
  } finally {
    if (child) {
      await sendCommand(s, controlCommands.Quit!, []).catch(() => {});
      s.end();
      await new Promise<void>((r) => child!.once("exit", () => r()));
    } else {
      s.end();
    }
  }
}

if (import.meta.main) await main();
