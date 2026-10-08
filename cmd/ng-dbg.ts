import { existsSync, readdirSync } from "node:fs";
import { delimiter, join } from "node:path";

// Build the debug ASan executable and run it under a debugger.
// Arguments after -- go to SumatraPDF. The launch saves and restores
// the session; pass -- -for-testing to skip that.
//
// Windows defaults to cdb. macOS tries lldb then gdb; Linux tries gdb then lldb.

const usage = `Usage: bun cmd/ng-dbg.ts [-cdb|-windbg|-lldb|-gdb] [-clean] [-- <SumatraPDF args>]

  -cdb      Windows: cdb.exe (default)
  -windbg   Windows: WinDbg
  -lldb     macOS/Linux: lldb
  -gdb      macOS/Linux: gdb
  -clean    delete the debug ASan output directory first

With no debugger flag, macOS uses lldb if it is installed and otherwise gdb.
Linux uses gdb if it is installed and otherwise lldb.`;

class CliError extends Error {}

type DebuggerKind = "cdb" | "windbg" | "lldb" | "gdb";
type HostPlat = "win" | "mac" | "linux";

type Options = {
  debugger: DebuggerKind | null;
  clean: boolean;
  appArgs: string[];
};

function parseArgs(args: string[]): Options | null {
  if (args.length === 1 && ["-h", "-help", "--help"].includes(args[0]!)) return null;

  const sep = args.indexOf("--");
  const ours = sep < 0 ? args : args.slice(0, sep);
  const appArgs = sep < 0 ? [] : args.slice(sep + 1);
  let debuggerKind: DebuggerKind | null = null;
  let clean = false;
  for (const arg of ours) {
    if (arg === "-cdb" || arg === "-windbg" || arg === "-lldb" || arg === "-gdb") {
      const next = arg.slice(1) as DebuggerKind;
      if (debuggerKind) throw new CliError("debugger option can only be specified once");
      debuggerKind = next;
    } else if (arg === "-clean") {
      if (clean) throw new CliError("-clean can only be specified once");
      clean = true;
    } else {
      throw new CliError(`unknown option: ${arg}`);
    }
  }
  return { debugger: debuggerKind, clean, appArgs };
}

function hostPlat(): HostPlat {
  if (process.platform === "win32") return "win";
  if (process.platform === "darwin") return "mac";
  if (process.platform === "linux") return "linux";
  throw new Error(`cmd/ng-dbg.ts does not support ${process.platform}`);
}

function findOnPath(name: string): string | null {
  for (const dir of (process.env.PATH ?? "").split(delimiter)) {
    const clean = dir.replaceAll('"', "");
    if (!clean) continue;
    if (process.platform === "win32" && clean.toLowerCase().includes("\\windowsapps")) continue;
    const path = join(clean, name);
    if (existsSync(path)) return path;
  }
  return null;
}

function debuggerKits(): string[] {
  return [
    String.raw`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64`,
    String.raw`C:\Program Files\Windows Kits\10\Debuggers\x64`,
  ];
}

function firstExisting(paths: string[]): string | null {
  return paths.find((path) => existsSync(path)) ?? null;
}

function findCdb(): string | null {
  return findOnPath("cdb.exe") ?? firstExisting(debuggerKits().map((dir) => join(dir, "cdb.exe")));
}

function registryOutput(args: string[]): string {
  const result = Bun.spawnSync(["reg", ...args]);
  if (result.exitCode !== 0 || !result.stdout) return "";
  return Buffer.from(result.stdout).toString("utf8");
}

function findStoreWinDbg(): string | null {
  const packagesKey =
    "HKCU\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
  for (const line of registryOutput(["query", packagesKey]).split(/\r?\n/)) {
    const key = line.trim();
    if (!/\\Microsoft\.WinDbg_/i.test(key)) continue;
    const output = registryOutput(["query", key, "/v", "PackageRootFolder"]);
    const root = output.match(/PackageRootFolder\s+REG_\w+\s+(.+)/i)?.[1]?.trim();
    if (!root) continue;
    const exe = join(root, "DbgX.Shell.exe");
    if (existsSync(exe)) return exe;
  }

  const appRoots = [
    String.raw`C:\Program Files\WindowsApps`,
    join(process.env.ProgramFiles ?? String.raw`C:\Program Files`, "WindowsApps"),
  ];
  for (const root of appRoots) {
    if (!existsSync(root)) continue;
    let names: string[];
    try {
      names = readdirSync(root);
    } catch {
      continue;
    }
    const packages = names
      .filter((name) => /^Microsoft\.WinDbg_.+_x64__/i.test(name))
      .sort()
      .reverse();
    for (const name of packages) {
      const exe = join(root, name, "DbgX.Shell.exe");
      if (existsSync(exe)) return exe;
    }
  }
  return null;
}

function findWinDbg(): string | null {
  return (
    findOnPath("windbgx.exe") ??
    findOnPath("windbg.exe") ??
    firstExisting(debuggerKits().map((dir) => join(dir, "windbg.exe"))) ??
    findStoreWinDbg()
  );
}

function debuggerFlags(kind: DebuggerKind): string[] {
  // ASan maps shadow memory with first-chance access violations. Ignore those;
  // e0736172 is another handled exception emitted during startup. ASan
  // failures still stop at their debug break.
  if (kind === "cdb") return ["-o", "-g", "-G", "-xi", "av", "-xi", "0xe0736172"];
  if (kind === "windbg") return ["-Q", "-o", "-G", "-c", "sxi av; sxi 0xe0736172; g"];
  // -o run / -ex run start the program. -- / --args keep the app's
  // arguments from being read as debugger options.
  if (kind === "lldb") return ["-o", "run", "--"];
  return ["-ex", "run", "--args"];
}

function findDebuggerExe(kind: DebuggerKind): string | null {
  if (kind === "cdb") return findCdb();
  if (kind === "windbg") return findWinDbg();
  return findOnPath(kind);
}

function pickDebugger(plat: HostPlat, requested: DebuggerKind | null): { kind: DebuggerKind; exe: string } {
  const windowsOnly = (kind: DebuggerKind) => kind === "cdb" || kind === "windbg";
  if (requested) {
    if (plat === "win" && !windowsOnly(requested))
      throw new Error(`-${requested} is only available on macOS and Linux`);
    if (plat !== "win" && windowsOnly(requested)) throw new Error(`-${requested} is only available on Windows`);
    const exe = findDebuggerExe(requested);
    if (!exe) throw new Error(`${requested} debugger not found`);
    return { kind: requested, exe };
  }
  const order: DebuggerKind[] = plat === "win" ? ["cdb"] : plat === "mac" ? ["lldb", "gdb"] : ["gdb", "lldb"];
  for (const kind of order) {
    const exe = findDebuggerExe(kind);
    if (exe) return { kind, exe };
  }
  throw new Error(`no debugger found (looked for ${order.join(" and ")})`);
}

// ASan prints the report and then calls _exit, so the debugger never stops.
// abort_on_error turns that into SIGABRT. On macOS the nano allocator holds
// the address range ASan needs (malloc: nano zone abandoned). Leave values
// the user already set.
function asanAbortEnv(plat: HostPlat): NodeJS.ProcessEnv | undefined {
  if (plat === "win") return undefined;
  const env = { ...process.env };
  let changed = false;
  const cur = env.ASAN_OPTIONS ?? "";
  if (!/(^|:)abort_on_error(=|:|$)/.test(cur)) {
    env.ASAN_OPTIONS = cur ? `${cur}:abort_on_error=1` : "abort_on_error=1";
    changed = true;
  }
  if (plat === "mac" && env.MallocNanoZone === undefined) {
    env.MallocNanoZone = "0";
    changed = true;
  }
  return changed ? env : undefined;
}

async function run(command: string[], description: string, env?: NodeJS.ProcessEnv): Promise<void> {
  console.log(`> ${command.join(" ")}`);
  const proc = Bun.spawn(command, { stdin: "inherit", stdout: "inherit", stderr: "inherit", env });
  const code = await proc.exited;
  if (code !== 0) throw new Error(`${description} failed with exit code ${code}`);
}

async function main(): Promise<void> {
  let opts: Options | null;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`error: ${message}\n`);
    console.error(usage);
    process.exitCode = 1;
    return;
  }
  if (!opts) {
    console.log(usage);
    return;
  }
  const plat = hostPlat();
  const dbg = pickDebugger(plat, opts.debugger);

  const buildArgs = ["bun", join(import.meta.dir, "ng-build.ts"), "-dbg", "-asan"];
  if (opts.clean) buildArgs.push("-clean");
  await run(buildArgs, "build");

  const exeName = plat === "win" ? "SumatraPDF.exe" : "SumatraPDF";
  const exe = join(process.cwd(), "out", plat, "dbg-asan", exeName);
  if (!existsSync(exe)) throw new Error(`debug ASan executable not found: ${exe}`);
  // A normal launch: it saves settings and restores the session.
  // A throwaway run passes -- -for-testing itself.
  await run([dbg.exe, ...debuggerFlags(dbg.kind), exe, ...opts.appArgs], dbg.kind, asanAbortEnv(plat));
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nDebug failed: ${message}`);
  process.exitCode = 1;
}
