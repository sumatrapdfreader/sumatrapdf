/**
 * Run the native Linux build from Windows by invoking cmd/build-linux.ts
 * inside WSL Ubuntu. Matches .github/workflows/linux-daily.yml
 * (`bun cmd/build-linux.ts -asan`).
 *
 * Usage:
 *   bun cmd/build-linux-in-wsl.ts                 # asan, same as Linux CI
 *   bun cmd/build-linux-in-wsl.ts -debug
 *   bun cmd/build-linux-in-wsl.ts -release
 *   bun cmd/build-linux-in-wsl.ts -asan -clean
 *
 * Requires:
 *   - WSL distro named "Ubuntu"
 *   - bun in the distro (curl -fsSL https://bun.sh/install | bash)
 *   - compilers: sudo apt install build-essential clang libssl-dev
 *   - for -asan with clang: sudo apt install libclang-rt-$(clang -dumpversion | cut -d. -f1)-dev
 *     (without this the Linux script falls back to g++, which uses libasan)
 *
 * Note: the Linux script is base64-encoded before being passed to wsl.exe so
 * that Windows/WSL argument processing cannot mangle `$`, quotes, or newlines.
 */

import { existsSync } from "node:fs";
import { join } from "node:path";

const WSL_DISTRO = "Ubuntu";
const BUILD_SCRIPT = "cmd/build-linux.ts";

function shellQuote(s: string): string {
  return `'${s.replace(/'/g, `'\\''`)}'`;
}

function windowsPathToWsl(winPath: string): string {
  // C:\foo\bar -> /mnt/c/foo/bar
  const m = /^([A-Za-z]):[\\/](.*)$/.exec(winPath);
  if (!m) {
    return winPath.replace(/\\/g, "/");
  }
  const drive = m[1].toLowerCase();
  const rest = m[2].replace(/\\/g, "/");
  return `/mnt/${drive}/${rest}`;
}

function requireWsl(): void {
  if (!Bun.which("wsl")) {
    console.error("wsl not found in PATH. Install WSL and the Ubuntu distro.");
    process.exit(1);
  }
}

function hasConfigFlag(args: string[]): boolean {
  return args.some((a) => a === "-debug" || a === "-release" || a === "-asan");
}

async function runLocal(args: string[]): Promise<void> {
  const proc = Bun.spawn(["bun", BUILD_SCRIPT, ...args], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    process.exit(exitCode);
  }
}

async function main(): Promise<void> {
  const forwarded = Bun.argv.slice(2);
  if (!hasConfigFlag(forwarded)) {
    forwarded.unshift("-asan");
  }

  const cwd = process.cwd();
  const buildScriptWin = join(cwd, ...BUILD_SCRIPT.split("/"));
  if (!existsSync(buildScriptWin)) {
    console.error(`Build script not found: ${buildScriptWin}`);
    console.error("Run this from the SumatraPDF repo root.");
    process.exit(1);
  }

  // Already on Linux (including WSL invoked as Linux): skip the wsl.exe hop.
  if (process.platform === "linux") {
    await runLocal(forwarded);
    return;
  }

  requireWsl();

  const wslCwd = windowsPathToWsl(cwd);
  const quotedArgs = forwarded.map(shellQuote).join(" ");

  // Keep this as plain string concatenation (not a template) where $ must stay
  // for bash; base64 below is the real defense against wsl.exe mangling.
  const remoteScript = [
    "set -euo pipefail",
    // WSL may inherit a Windows HOME; always take the Linux home from passwd.
    'export HOME="$(getent passwd "$(id -un)" | cut -d: -f6)"',
    `cd ${shellQuote(wslCwd)}`,
    // Drop Windows PATH interop entries; keep a minimal Linux PATH + bun install dir.
    'export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$HOME/.bun/bin"',
    'BUN=""',
    'if [ -x "$HOME/.bun/bin/bun" ]; then BUN="$HOME/.bun/bin/bun"; fi',
    'if [ -z "$BUN" ] && command -v bun >/dev/null 2>&1; then BUN="$(command -v bun)"; fi',
    'if [ -z "$BUN" ]; then',
    '  echo "bun not found in WSL (looked in $HOME/.bun/bin/bun)." >&2',
    '  echo "Install with: curl -fsSL https://bun.sh/install | bash" >&2',
    '  echo "Also need: sudo apt install build-essential clang libssl-dev" >&2',
    "  exit 1",
    "fi",
    `echo "> wsl -d ${WSL_DISTRO}: $BUN ${BUILD_SCRIPT}${quotedArgs ? " " + quotedArgs : ""}"`,
    `exec "$BUN" ${BUILD_SCRIPT}${quotedArgs ? " " + quotedArgs : ""}`,
    "",
  ].join("\n");

  // Base64 so wsl.exe / CreateProcess cannot strip or expand `$`, quotes, etc.
  const b64 = Buffer.from(remoteScript, "utf8").toString("base64");
  const wrapper = `echo ${b64} | base64 -d | bash -l`;

  console.log(`> wsl -d ${WSL_DISTRO}: bun ${BUILD_SCRIPT}${forwarded.length ? " " + forwarded.join(" ") : ""}`);

  const proc = Bun.spawn(["wsl", "-d", WSL_DISTRO, "-e", "bash", "-lc", wrapper], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });

  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    process.exit(exitCode);
  }
}

if (import.meta.main) {
  main().catch((e: unknown) => {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`\nWSL Linux build failed: ${msg}`);
    process.exit(1);
  });
}
