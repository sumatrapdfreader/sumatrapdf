// Runs cmd/ng-build.ts in WSL Ubuntu against the same checkout, without -linux.
//
// The inner script is base64-encoded so neither wsl.exe's argument handling
// nor bash can mangle quotes, `$` or newlines.

const distro = "Ubuntu";

const bunHint = [
  "bun was not found in WSL (looked in $HOME/.bun/bin/bun).",
  "Install it with: curl -fsSL https://bun.sh/install | bash",
  "The build also needs: sudo apt install build-essential pkg-config libx11-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev libssl-dev",
].join("\\n");

function shellQuote(s: string): string {
  return `'${s.replaceAll("'", `'\\''`)}'`;
}

function toWslPath(winPath: string): string {
  const m = /^([A-Za-z]):[\\/](.*)$/.exec(winPath);
  if (!m) return winPath.replaceAll("\\", "/");
  return `/mnt/${m[1]!.toLowerCase()}/${m[2]!.replaceAll("\\", "/")}`;
}

/** Runs `bun cmd/ng-build.ts <args>` inside WSL. Returns its exit code. */
export async function runBuildInWsl(root: string, args: string[]): Promise<number> {
  if (!Bun.which("wsl")) {
    console.error("wsl.exe not found. Install WSL and the Ubuntu distro.");
    return 1;
  }
  const quoted = args.map(shellQuote).join(" ");
  const script = [
    "set -euo pipefail",
    `cd ${shellQuote(toWslPath(root))}`,
    'export PATH="$HOME/.bun/bin:$PATH"',
    "if ! command -v bun >/dev/null 2>&1; then",
    `  printf '%b\\n' ${shellQuote(bunHint)} >&2`,
    "  exit 1",
    "fi",
    `exec bun cmd/ng-build.ts ${quoted}`,
    "",
  ].join("\n");
  const b64 = Buffer.from(script, "utf8").toString("base64");
  console.log(`> wsl -d ${distro}: bun cmd/ng-build.ts ${args.join(" ")}`);
  const p = Bun.spawn(["wsl", "-d", distro, "-e", "bash", "-lc", `echo ${b64} | base64 -d | bash`], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  return p.exited;
}
