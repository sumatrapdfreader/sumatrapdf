import { Socket, createConnection } from "node:net";

export enum ControlCommand {
  Ping = 1,
  Quit = 2,
  TestSynctex = 10,
  TestSearch = 11,
  TestDest = 12,
  TestNamedDest = 13,
  TestChm = 14,
  TestSelectionTranslate = 15,
  TestTripleClickLineSelect = 16,
  TestContextMenuSelection = 17,
  TestGoToFindMatch = 18,
  // IDs 19-21 unused (reserved on the -dbg-control wire protocol; do not renumber).
  // Assign new test commands starting at 23.
  TestInverseSearch = 22,
  TestImageResizeArrowKey = 23,
  TestFindResultPageColumnClip = 24,
  TestFileKind = 25,
  TestScrollToLink = 26,
  TestI18nErrorString = 27,
  TestPageInfoOverlay = 28,
  TestGetToc = 29,
  TestPageLinks = 30,
}

export type ControlArg = number | string | Uint8Array | ControlArg[];

const enum ArgType {
  End = 0,
  Int32 = 1,
  Bytes = 2,
  String = 3,
  List = 4,
}

function appendU16(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff);
}

function appendU32(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}

function appendBytes(out: number[], bytes: Uint8Array): void {
  for (const b of bytes) {
    out.push(b);
  }
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
    appendBytes(out, bytes);
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
  appendBytes(out, arg);
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
    const zero = r.bytes(1)[0];
    if (zero !== 0) {
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

function pipePath(pipeName: string): string {
  return pipeName.startsWith("\\\\.\\pipe\\") ? pipeName : `\\\\.\\pipe\\${pipeName}`;
}

async function readExactly(socket: Socket, len: number): Promise<Buffer> {
  const chunks: Buffer[] = [];
  let total = 0;
  while (total < len) {
    const chunk = socket.read(len - total) as Buffer | null;
    if (chunk) {
      chunks.push(chunk);
      total += chunk.length;
      continue;
    }
    await waitForReadable(socket);
  }
  return Buffer.concat(chunks, total);
}

function waitForReadable(socket: Socket): Promise<void> {
  // if the pipe already closed (e.g. the app died on a debug report), the
  // close/end events fired in the past and will never fire again -- reject
  // instead of waiting forever
  if (socket.destroyed || socket.readableEnded) {
    return Promise.reject(new Error("control pipe closed while reading"));
  }
  return new Promise((resolve, reject) => {
    const cleanup = () => {
      socket.off("readable", onReadable);
      socket.off("end", onEnd);
      socket.off("close", onClose);
      socket.off("error", onError);
    };
    const onReadable = () => {
      cleanup();
      resolve();
    };
    const onEnd = () => {
      cleanup();
      reject(new Error("control pipe closed while reading"));
    };
    const onClose = () => {
      cleanup();
      reject(new Error("control pipe closed while reading"));
    };
    const onError = (err: Error) => {
      cleanup();
      reject(err);
    };
    socket.once("readable", onReadable);
    socket.once("end", onEnd);
    socket.once("close", onClose);
    socket.once("error", onError);
  });
}

function connectSocket(path: string): Promise<Socket> {
  return new Promise((resolve, reject) => {
    const socket = createConnection(path);
    const cleanup = () => {
      socket.off("connect", onConnect);
      socket.off("error", onError);
    };
    const onConnect = () => {
      cleanup();
      resolve(socket);
    };
    const onError = (err: Error) => {
      cleanup();
      socket.destroy();
      reject(err);
    };
    socket.once("connect", onConnect);
    socket.once("error", onError);
  });
}

function cleanEnv(env: Record<string, string | undefined> | undefined): Record<string, string> | undefined {
  if (!env) {
    return undefined;
  }
  const res: Record<string, string> = {};
  for (const [key, value] of Object.entries(env)) {
    if (value !== undefined) {
      res[key] = value;
    }
  }
  return res;
}

export class ControlClient {
  private nextId = 1;

  constructor(readonly socket: Socket) {}

  static async connect(pipeName: string, timeoutMs = 10000): Promise<ControlClient> {
    const path = pipePath(pipeName);
    const deadline = Date.now() + timeoutMs;
    let lastErr: unknown;
    while (Date.now() < deadline) {
      try {
        const socket = await connectSocket(path);
        return new ControlClient(socket);
      } catch (e) {
        lastErr = e;
        await new Promise((resolve) => setTimeout(resolve, 50));
      }
    }
    throw new Error(`failed to connect to Sumatra control pipe ${path}: ${lastErr}`);
  }

  async request(cmd: ControlCommand, args: ControlArg[] = []): Promise<ControlArg[]> {
    if (this.socket.destroyed) {
      throw new Error("control pipe closed");
    }
    const id = this.nextId++ & 0xffff;
    this.socket.write(encodeRequest(cmd, id, args));

    const sizeBuf = await readExactly(this.socket, 4);
    const size = sizeBuf.readUInt32LE(0);
    const payload = await readExactly(this.socket, size);
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

  async quit(): Promise<void> {
    await this.request(ControlCommand.Quit);
  }

  close(): void {
    this.socket.end();
  }
}

export function uniquePipeName(prefix = "sumatra-control"): string {
  return `${prefix}-${process.pid}-${Date.now()}-${Math.floor(Math.random() * 0x100000000).toString(16)}`;
}

// exit code SumatraPDF uses when a debug report (ReportIf) fires in a
// -for-testing run; must match kDebugReportTestExitCode in src/CrashHandler.cpp
export const DEBUG_REPORT_EXIT_CODE = 105;

export async function withControlledSumatra<T>(
  exe: string,
  fn: (client: ControlClient) => Promise<T>,
  extraArgs: string[] = [],
  options: { cwd?: string; env?: Record<string, string | undefined>; connectTimeoutMs?: number } = {},
): Promise<T> {
  const pipeName = uniquePipeName();
  // stderr is piped: on a debug report the app writes the report text there
  // before terminating, and we surface it in the failure below
  const proc = Bun.spawn([exe, "-for-testing", "-dbg-control", pipeName, ...extraArgs], {
    stdout: "ignore",
    stderr: "pipe",
    cwd: options.cwd,
    env: cleanEnv(options.env),
  });
  let client: ControlClient | undefined;
  let killed = false;
  let result: T | undefined;
  let fnErr: unknown;
  let fnOk = false;
  try {
    client = await ControlClient.connect(pipeName, options.connectTimeoutMs ?? 10000);
    result = await fn(client);
    fnOk = true;
  } catch (e) {
    fnErr = e;
  } finally {
    if (client) {
      try {
        await client.quit();
      } catch {
        proc.kill();
        killed = true;
      }
      client.close();
    } else {
      proc.kill();
      killed = true;
    }
  }
  // quit is graceful (CmdExit) and could in theory be blocked by a dialog;
  // don't let a test hang forever waiting for the process to go away
  let exitTimer: ReturnType<typeof setTimeout> | undefined;
  const exitCode = await Promise.race([
    proc.exited,
    new Promise<"timeout">((resolve) => {
      exitTimer = setTimeout(() => resolve("timeout"), 30_000);
    }),
  ]);
  clearTimeout(exitTimer);
  if (exitCode === "timeout") {
    proc.kill();
    await proc.exited;
    throw new Error("SumatraPDF did not exit within 30s of Quit");
  }
  const stderrText = proc.stderr ? (await new Response(proc.stderr).text()).trim() : "";
  if (!fnOk) {
    if (stderrText) {
      console.error(stderrText);
    }
    throw fnErr;
  }
  // a debug report (ReportIf) or crash during the run / shutdown must fail the test
  if (!killed && exitCode !== 0) {
    const what = exitCode === DEBUG_REPORT_EXIT_CODE ? "debug report (ReportIf) fired" : `exit code ${exitCode}`;
    const details = stderrText ? `\n${stderrText}` : "";
    throw new Error(`SumatraPDF: ${what}${details}`);
  }
  return result as T;
}

export async function runControlCommand(
  exe: string,
  cmd: ControlCommand,
  args: ControlArg[] = [],
  extraArgs: string[] = [],
): Promise<ControlArg[]> {
  return await withControlledSumatra(exe, (client) => client.request(cmd, args), extraArgs);
}

if (import.meta.main) {
  const [exe, cmdName, ...args] = process.argv.slice(2);
  if (!exe || !cmdName) {
    console.log("Usage: bun cmd/control.ts <SumatraPDF.exe> <ping|test-search> [args...]");
    process.exit(1);
  }
  const cmd = cmdName === "ping" ? ControlCommand.Ping : cmdName === "test-search" ? ControlCommand.TestSearch : 0;
  if (!cmd) {
    throw new Error(`unknown command: ${cmdName}`);
  }
  const res = await runControlCommand(exe, cmd, args);
  console.log(JSON.stringify(res));
}
