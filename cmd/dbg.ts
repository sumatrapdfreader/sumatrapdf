import { existsSync, readdirSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { runLogged } from "./util";

// Builds the debug ASan build and runs it under a debugger, so a bad access
// stops in the debugger with ASan's report rather than somewhere later on.
//
// Usage:
//   bun cmd/dbg.ts                            # cdb if available, else WinDbg
//   bun cmd/dbg.ts -windbg                    # WinDbg GUI
//   bun cmd/dbg.ts -- -for-testing foo.pdf    # args after -- go to SumatraPDF
//   bun cmd/dbg.ts -windbg -- -for-testing foo.pdf
//
// Pass -for-testing when opening a document to poke at: it starts a fresh
// instance and won't overwrite the settings you actually use.

const exePath = join("out", "dbg64_asan", "SumatraPDF-static.exe");

function firstExisting(paths: string[]): string | null {
  return paths.find((p) => existsSync(p)) ?? null;
}

function spawnText(cmd: string, args: string[]): { exitCode: number | null; stdout: string } {
  const r = Bun.spawnSync([cmd, ...args]);
  return { exitCode: r.exitCode, stdout: r.stdout ? Buffer.from(r.stdout).toString("utf8") : "" };
}

function findOnPath(name: string): string | null {
  for (const dir of (process.env.PATH ?? "").split(";")) {
    const clean = dir.replaceAll('"', "");
    if (!clean) {
      continue;
    }
    // AppX execution aliases under WindowsApps are 0-byte reparse points:
    // existsSync is false and spawn can't launch them. Skip that dir and look
    // them up via the package registry instead.
    if (clean.toLowerCase().includes("\\windowsapps")) {
      continue;
    }
    const path = join(clean, name);
    if (existsSync(path)) {
      return path;
    }
  }
  return null;
}

// Store WinDbg (WinDbgX) registers under AppModel Packages. The executable is
// DbgX.Shell.exe; WinDbgX.exe is only an app-execution alias that bun cannot
// spawn.
function findWinDbgFromRegistry(): string | null {
  const packagesKey =
    "HKCU\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
  const listed = spawnText("reg", ["query", packagesKey]);
  if (listed.exitCode !== 0 || !listed.stdout) {
    return null;
  }
  for (const line of listed.stdout.split(/\r?\n/)) {
    const key = line.trim();
    if (!/\\Microsoft\.WinDbg_/i.test(key)) {
      continue;
    }
    const prop = spawnText("reg", ["query", key, "/v", "PackageRootFolder"]);
    if (prop.exitCode !== 0 || !prop.stdout) {
      continue;
    }
    const root = prop.stdout.match(/PackageRootFolder\s+REG_\w+\s+(.+)/i)?.[1]?.trim();
    if (!root) {
      continue;
    }
    const shell = join(root, "DbgX.Shell.exe");
    if (existsSync(shell)) {
      return shell;
    }
  }
  return null;
}

// Same package, without the registry: scan WindowsApps for the install folder.
// Needs read access to Program Files\WindowsApps (usually works for the owner).
function findWinDbgFromWindowsApps(): string | null {
  const roots = [
    String.raw`C:\Program Files\WindowsApps`,
    join(process.env.ProgramFiles ?? String.raw`C:\Program Files`, "WindowsApps"),
  ];
  for (const root of roots) {
    if (!existsSync(root)) {
      continue;
    }
    let names: string[];
    try {
      names = readdirSync(root);
    } catch {
      continue;
    }
    // prefer the highest version-looking folder; names look like
    // Microsoft.WinDbg_1.2606.22001.0_x64__8wekyb3d8bbwe
    const pkgs = names
      .filter((n) => /^Microsoft\.WinDbg_.+_x64__/i.test(n))
      .sort()
      .reverse();
    for (const n of pkgs) {
      const shell = join(root, n, "DbgX.Shell.exe");
      if (existsSync(shell)) {
        return shell;
      }
    }
  }
  return null;
}

function debuggerKits(): string[] {
  return [
    String.raw`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64`,
    String.raw`C:\Program Files\Windows Kits\10\Debuggers\x64`,
  ];
}

function findCdb(): string | null {
  return findOnPath("cdb.exe") ?? firstExisting(debuggerKits().map((d) => join(d, "cdb.exe")));
}

function findWinDbg(): string | null {
  return (
    findOnPath("windbgx.exe") ??
    findOnPath("windbg.exe") ??
    firstExisting(debuggerKits().map((d) => join(d, "windbg.exe"))) ??
    findWinDbgFromRegistry() ??
    findWinDbgFromWindowsApps()
  );
}

// -o debug child processes, -G don't break on exit. ASan maps shadow memory via
// first-chance access violations (passed through, not handled); ignore them so
// the debugger does not print a line per fault. Real ASan errors still stop on
// __debugbreak. -Q (don't save settings) is WinDbg GUI only.
//
// cdb: -xi av is a startup switch, so it is in effect before the first AV.
// WinDbg: -c "sxi av; g" runs at the initial breakpoint then continues (same
// as -g, but the ignore is set first). Classic windbg has no -xi switch.
const cdbFlags = ["-o", "-g", "-G", "-xi", "av"];
const windbgFlags = ["-Q", "-o", "-G", "-c", "sxi av; g"];

// The debugger to use plus the flags that make it start the program right away
// instead of stopping at the initial breakpoint. Prefer cdb (console) over the
// WinDbg GUI when it's installed, unless -windbg was passed; both beat raddbg
// because they understand the ASan report.
export function findDebugger(preferWindbg = false): { exe: string; flags: string[] } | null {
  if (!preferWindbg) {
    const cdb = findCdb();
    if (cdb) {
      return { exe: cdb, flags: cdbFlags };
    }
  }

  const windbg = findWinDbg();
  if (windbg) {
    return { exe: windbg, flags: windbgFlags };
  }

  if (preferWindbg) {
    return null;
  }

  const raddbg = firstExisting([join(homedir(), "OneDrive", "bin", "raddbg.exe")]) ?? findOnPath("raddbg.exe");
  if (raddbg) {
    return { exe: raddbg, flags: ["--auto_run"] };
  }
  return null;
}

// -windbg is ours; arguments after "--" (or leftover args, if there's no "--")
// go to the app.
function parseCli(): { preferWindbg: boolean; app: string[] } {
  const args = process.argv.slice(2);
  const sep = args.indexOf("--");
  const ours = sep < 0 ? args : args.slice(0, sep);
  const afterSep = sep < 0 ? [] : args.slice(sep + 1);

  let preferWindbg = false;
  const rest: string[] = [];
  for (const a of ours) {
    if (a === "-windbg") {
      preferWindbg = true;
    } else {
      rest.push(a);
    }
  }
  return { preferWindbg, app: sep < 0 ? rest : afterSep };
}

async function main() {
  const { preferWindbg, app } = parseCli();
  await runLogged("bun", [join(import.meta.dir, "build.ts"), "-asan"]);

  const dbg = findDebugger(preferWindbg);
  if (!dbg) {
    if (preferWindbg) {
      console.error(`no WinDbg found: looked on PATH, Windows Kits, and the Store package`);
    } else {
      console.error(
        `no debugger found: looked for cdb/windbg/WinDbgX (PATH, Windows Kits, Store package) and ${join(homedir(), "OneDrive", "bin", "raddbg.exe")}`,
      );
    }
    process.exit(1);
  }

  // bun kills its children when it exits, so this waits for the debugger
  // rather than launching it and returning
  const args = [...dbg.flags, join(process.cwd(), exePath), ...app];
  console.log(`> ${dbg.exe} ${args.join(" ")}`);
  const res = Bun.spawnSync([dbg.exe, ...args], { stdio: ["inherit", "inherit", "inherit"] });
  process.exit(res.exitCode ?? 0);
}

if (import.meta.main) {
  await main();
}
