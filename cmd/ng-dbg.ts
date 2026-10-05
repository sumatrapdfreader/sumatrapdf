import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";

// Build the debug ASan executable and run it under the Windows debugger.
// Arguments after -- go to SumatraPDF.

const usage = `Usage: bun cmd/ng-dbg.ts [-cdb|-windbg] [-clean] [-- <SumatraPDF args>]

  -cdb      use cdb.exe (default)
  -windbg   use WinDbg
  -clean    delete the debug ASan output directory first`;

class CliError extends Error {}

type DebuggerKind = "cdb" | "windbg";

type Options = {
  debugger: DebuggerKind;
  clean: boolean;
  appArgs: string[];
};

function parseArgs(args: string[]): Options | null {
  if (args.length === 1 && ["-h", "-help", "--help"].includes(args[0]!)) return null;

  const sep = args.indexOf("--");
  const ours = sep < 0 ? args : args.slice(0, sep);
  const appArgs = sep < 0 ? [] : args.slice(sep + 1);
  let debuggerKind: DebuggerKind = "cdb";
  let debuggerSet = false;
  let clean = false;
  for (const arg of ours) {
    if (arg === "-cdb" || arg === "-windbg") {
      const next = arg.slice(1) as DebuggerKind;
      if (debuggerSet) throw new CliError("debugger option can only be specified once");
      debuggerKind = next;
      debuggerSet = true;
    } else if (arg === "-clean") {
      if (clean) throw new CliError("-clean can only be specified once");
      clean = true;
    } else {
      throw new CliError(`unknown option: ${arg}`);
    }
  }
  return { debugger: debuggerKind, clean, appArgs };
}

function findOnPath(name: string): string | null {
  for (const dir of (process.env.PATH ?? "").split(";")) {
    const clean = dir.replaceAll('"', "");
    if (!clean || clean.toLowerCase().includes("\\windowsapps")) continue;
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

async function run(command: string[], description: string): Promise<void> {
  console.log(`> ${command.join(" ")}`);
  const proc = Bun.spawn(command, { stdin: "inherit", stdout: "inherit", stderr: "inherit" });
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
  if (process.platform !== "win32") throw new Error("cmd/ng-dbg.ts only supports Windows");

  const buildArgs = ["bun", join(import.meta.dir, "ng-build.ts"), "-dbg", "-asan"];
  if (opts.clean) buildArgs.push("-clean");
  await run(buildArgs, "build");

  const debuggerExe = opts.debugger === "cdb" ? findCdb() : findWinDbg();
  if (!debuggerExe) throw new Error(`${opts.debugger} debugger not found`);

  // ASan maps shadow memory with first-chance access violations. Ignore those;
  // e0736172 is another handled exception emitted during startup. ASan
  // failures still stop at their debug break.
  const flags =
    opts.debugger === "cdb"
      ? ["-o", "-g", "-G", "-xi", "av", "-xi", "0xe0736172"]
      : ["-Q", "-o", "-G", "-c", "sxi av; sxi 0xe0736172; g"];
  const exe = join(process.cwd(), "out", "win", "dbg-asan", "SumatraPDF.exe");
  await run([debuggerExe, ...flags, exe, "-for-testing", ...opts.appArgs], opts.debugger);
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nDebug failed: ${message}`);
  process.exitCode = 1;
}
