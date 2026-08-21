import { existsSync, readdirSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { runLogged } from "./util";

// Builds the debug ASan build and runs it under a debugger, so a bad access
// stops in the debugger with ASan's report rather than somewhere later on.
//
// Usage:
//   bun cmd/dbg.ts                            # just start it
//   bun cmd/dbg.ts -- -for-testing foo.pdf    # args after -- go to SumatraPDF
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

// The debugger to use plus the flags that make it start the program right away
// instead of stopping at the initial breakpoint. windbg / WinDbgX wins over
// raddbg because it understands the ASan report.
export function findDebugger(): { exe: string; flags: string[] } | null {
  // windbg: -Q quiet (don't save settings), -o debug child processes,
  // -g don't break on start, -G don't break on exit either, so the debugger
  // closes with the program instead of sitting on the final breakpoint.
  const windbg =
    findOnPath("windbgx.exe") ??
    findOnPath("windbg.exe") ??
    firstExisting([
      String.raw`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe`,
      String.raw`C:\Program Files\Windows Kits\10\Debuggers\x64\windbg.exe`,
    ]) ??
    findWinDbgFromRegistry() ??
    findWinDbgFromWindowsApps();
  if (windbg) {
    return { exe: windbg, flags: ["-Q", "-o", "-g", "-G"] };
  }

  const raddbg = firstExisting([join(homedir(), "OneDrive", "bin", "raddbg.exe")]) ?? findOnPath("raddbg.exe");
  if (raddbg) {
    return { exe: raddbg, flags: ["--auto_run"] };
  }
  return null;
}

// arguments after "--" (or all of them, if there's no "--") go to the app
function appArgs(): string[] {
  const args = process.argv.slice(2);
  const sep = args.indexOf("--");
  return sep < 0 ? args : args.slice(sep + 1);
}

async function main() {
  await runLogged("bun", [join(import.meta.dir, "build.ts"), "-asan"]);

  const dbg = findDebugger();
  if (!dbg) {
    console.error(
      `no debugger found: looked for windbg/WinDbgX (PATH, Windows Kits, Store package) and ${join(homedir(), "OneDrive", "bin", "raddbg.exe")}`,
    );
    process.exit(1);
  }

  // bun kills its children when it exits, so this waits for the debugger
  // rather than launching it and returning
  const args = [...dbg.flags, join(process.cwd(), exePath), ...appArgs()];
  console.log(`> ${dbg.exe} ${args.join(" ")}`);
  const res = Bun.spawnSync([dbg.exe, ...args], { stdio: ["inherit", "inherit", "inherit"] });
  process.exit(res.exitCode ?? 0);
}

if (import.meta.main) {
  await main();
}
