/**
 * Cross-compile the Windows SumatraPDF.exe from Windows by running
 * cmd/build-linux-wine.ts inside WSL Ubuntu (mingw-w64).
 *
 * Usage:
 *   bun cmd/build-win-in-wsl.ts              # debug static build -> out/dbg64-wine/SumatraPDF.exe
 *   bun cmd/build-win-in-wsl.ts -clean       # clean out/dbg64-wine (preserve settings)
 *   bun cmd/build-win-in-wsl.ts -run         # build, then run under Wine inside WSL
 *   bun cmd/build-win-in-wsl.ts -clean -run
 *   bun cmd/build-win-in-wsl.ts -- extra exe args...
 *
 * Requires:
 *   - WSL distro named "Ubuntu"
 *   - bun in the distro (curl -fsSL https://bun.sh/install | bash)
 *   - mingw-w64: sudo apt install g++-mingw-w64-x86-64
 *   - wine (only for -run): sudo apt install wine wine64
 *
 * Note: the Linux script is base64-encoded before being passed to wsl.exe so
 * that Windows/WSL argument processing cannot mangle `$`, quotes, or newlines.
 */

import { existsSync } from "node:fs";
import { join } from "node:path";

const WSL_DISTRO = "Ubuntu";
const BUILD_SCRIPT = "cmd/build-linux-wine.ts";

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

async function main(): Promise<void> {
  requireWsl();

  const forwarded = Bun.argv.slice(2);
  const cwd = process.cwd();
  const wslCwd = windowsPathToWsl(cwd);

  const buildScriptWin = join(cwd, ...BUILD_SCRIPT.split("/"));
  if (!existsSync(buildScriptWin)) {
    console.error(`Build script not found: ${buildScriptWin}`);
    console.error("Run this from the SumatraPDF repo root.");
    process.exit(1);
  }

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
    '  echo "Also need: sudo apt install g++-mingw-w64-x86-64 unzip" >&2',
    "  exit 1",
    "fi",
    `echo "> wsl -d ${WSL_DISTRO}: $BUN ${BUILD_SCRIPT}${quotedArgs ? " " + quotedArgs : ""}"`,
    `exec "$BUN" ${BUILD_SCRIPT}${quotedArgs ? " " + quotedArgs : ""}`,
    "",
  ].join("\n");

  // Base64 so wsl.exe / CreateProcess cannot strip or expand `$`, quotes, etc.
  const b64 = Buffer.from(remoteScript, "utf8").toString("base64");
  // Decode and eval in bash. -e forces direct exec (no intermediate shell parse of args).
  const wrapper = `echo ${b64} | base64 -d | bash -l`;

  console.log(
    `> wsl -d ${WSL_DISTRO}: bun ${BUILD_SCRIPT}${forwarded.length ? " " + forwarded.join(" ") : ""}`,
  );

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
    console.error(`\nWSL wine build failed: ${msg}`);
    process.exit(1);
  });
}
