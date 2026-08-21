/**
 * Build the Windows exe with MinGW from Windows, through WSL Ubuntu.
 *
 * Usage:
 *   bun cmd/helper/wsl-build.ts -win [options]
 *
 *   -win                Windows exe via mingw in WSL (cmd/build-linux-wine.ts)
 *     -clean            clean out/dbg64-wine (preserve settings)
 *     -run              run the exe under Wine after building
 *     -- args...        extra args for the exe (with -run)
 *
 * Requires a WSL distro named "Ubuntu" and bun in that distro.
 * Deps: sudo apt install g++-mingw-w64-x86-64 unzip
 *       (and wine wine64 for -run)
 *
 * The inner script is base64-encoded before being passed to wsl.exe so
 * Windows/WSL argument processing cannot mangle `$`, quotes, or newlines.
 */

import { existsSync } from "node:fs";
import { join } from "node:path";

const WSL_DISTRO = "Ubuntu";

const WIN_SCRIPT = "cmd/build-linux-wine.ts";

const WIN_FLAGS = new Set(["-clean", "-run"]);

function usage(): string {
  return `Usage: bun cmd/helper/wsl-build.ts -win [options]

  -win                Windows exe via mingw in WSL (cmd/build-linux-wine.ts)
    -clean            clean out/dbg64-wine (preserve settings)
    -run              run the exe under Wine after building
    -- args...        extra args for the exe (with -run)`;
}

function die(msg?: string): never {
  if (msg) {
    console.error(msg);
    console.error();
  }
  console.error(usage());
  process.exit(1);
}

function parseArgs(argv: string[]): { forwarded: string[] } {
  if (argv.length === 0) {
    die();
  }

  let sawTarget = false;
  const forwarded: string[] = [];
  let afterDashDash = false;

  for (const a of argv) {
    if (afterDashDash) {
      forwarded.push(a);
      continue;
    }
    if (a === "--") {
      forwarded.push(a);
      afterDashDash = true;
      continue;
    }
    if (a === "-win") {
      sawTarget = true;
      continue;
    }
    if (WIN_FLAGS.has(a)) {
      forwarded.push(a);
      continue;
    }
    die(`error: unknown argument: ${a}`);
  }

  if (!sawTarget) {
    die("error: missing -win");
  }

  return { forwarded };
}

function shellQuote(s: string): string {
  return `'${s.replace(/'/g, `'\\''`)}'`;
}

function windowsPathToWsl(winPath: string): string {
  const m = /^([A-Za-z]):[\\/](.*)$/.exec(winPath);
  if (!m) {
    return winPath.replace(/\\/g, "/");
  }
  return `/mnt/${m[1].toLowerCase()}/${m[2].replace(/\\/g, "/")}`;
}

function requireWsl(): void {
  if (!Bun.which("wsl")) {
    console.error("wsl not found in PATH. Install WSL and the Ubuntu distro.");
    process.exit(1);
  }
}

function bunMissingHint(): string[] {
  return [
    '  echo "bun not found in WSL (looked in $HOME/.bun/bin/bun)." >&2',
    '  echo "Install with: curl -fsSL https://bun.sh/install | bash" >&2',
    '  echo "Also need: sudo apt install g++-mingw-w64-x86-64 unzip" >&2',
  ];
}

async function runLocal(script: string, args: string[]): Promise<void> {
  const proc = Bun.spawn(["bun", script, ...args], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    process.exit(exitCode);
  }
}

async function runInWsl(script: string, args: string[]): Promise<void> {
  requireWsl();

  const cwd = process.cwd();
  const wslCwd = windowsPathToWsl(cwd);
  const quotedArgs = args.map(shellQuote).join(" ");

  const remoteScript = [
    "set -euo pipefail",
    'export HOME="$(getent passwd "$(id -un)" | cut -d: -f6)"',
    `cd ${shellQuote(wslCwd)}`,
    'export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$HOME/.bun/bin"',
    'BUN=""',
    'if [ -x "$HOME/.bun/bin/bun" ]; then BUN="$HOME/.bun/bin/bun"; fi',
    'if [ -z "$BUN" ] && command -v bun >/dev/null 2>&1; then BUN="$(command -v bun)"; fi',
    'if [ -z "$BUN" ]; then',
    ...bunMissingHint(),
    "  exit 1",
    "fi",
    `echo "> wsl -d ${WSL_DISTRO}: $BUN ${script}${quotedArgs ? " " + quotedArgs : ""}"`,
    `exec "$BUN" ${script}${quotedArgs ? " " + quotedArgs : ""}`,
    "",
  ].join("\n");

  const b64 = Buffer.from(remoteScript, "utf8").toString("base64");
  const wrapper = `echo ${b64} | base64 -d | bash -l`;

  console.log(`> wsl -d ${WSL_DISTRO}: bun ${script}${args.length ? " " + args.join(" ") : ""}`);

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

async function main(): Promise<void> {
  const { forwarded } = parseArgs(Bun.argv.slice(2));
  const script = WIN_SCRIPT;
  const args = forwarded;

  const cwd = process.cwd();
  const scriptWin = join(cwd, ...script.split("/"));
  if (!existsSync(scriptWin)) {
    console.error(`Build script not found: ${scriptWin}`);
    console.error("Run this from the SumatraPDF repo root.");
    process.exit(1);
  }

  if (process.platform === "linux") {
    await runLocal(script, args);
    return;
  }

  await runInWsl(script, args);
}

if (import.meta.main) {
  main().catch((e: unknown) => {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`\nWSL build failed: ${msg}`);
    process.exit(1);
  });
}
