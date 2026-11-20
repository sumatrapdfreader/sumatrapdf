// Builds SumatraPDF ng under Fedora, then runs or installs it. Shared by
// cmd/fedora-run.ts and cmd/fedora-install.ts.
//
// Everything happens in one bash script: on Fedora it runs directly, on
// Windows inside the Fedora WSL distro, against the same checkout.
//
//   packages (dnf) -> bun -> bun cmd/ng-build.ts -static -> run | install

import { readFileSync } from "node:fs";
import { linuxOutEnv, root } from "./ng-compile";
import { defaultTarget } from "./ng-targets";
import { shellQuote, toWslPath } from "./ng-wsl";

export type FedoraAction = "run" | "install";

// its own output dir: Ubuntu builds the same checkout into out/linux
const outPlat = "fedora";

const installDir = "/usr/local/bin";

const wslDistroPrefix = "Fedora";

const packages = [
  // toolchain
  "gcc",
  "gcc-c++",
  "pkgconf-pkg-config",
  "libstdc++-static",
  // the build stamps the git commit; bun's installer unpacks a zip
  "git",
  "unzip",
  // GPUI and the app link these
  "libX11-devel",
  "libXcursor-devel",
  "cairo-devel",
  "pango-devel",
  "gdk-pixbuf2-devel",
  "glib2-devel",
  "fontconfig-devel",
  "openssl-devel",
  "libcurl-devel",
  // the WSL image ships no fonts; GPUI falls back to these
  "dejavu-sans-fonts",
  "dejavu-sans-mono-fonts",
];

type Options = {
  debug: boolean;
  clean: boolean;
  runArgs: string[];
};

class CliError extends Error {}

function usage(action: FedoraAction): string {
  const script = `cmd/fedora-${action}.ts`;
  const what =
    action === "run" ? "runs it" : `copies it to ${installDir}/${defaultTarget}, so that \`${defaultTarget}\` runs it`;
  const lines = [
    `Usage: bun ${script} [-dbg] [-clean]${action === "run" ? " [[--] <run args>]" : ""}`,
    "",
    `Builds SumatraPDF under Fedora (from Windows: in the Fedora WSL distro) and ${what}.`,
    "Installs the packages and bun the build needs. libstdc++ and libgcc are linked",
    "statically; the GUI libraries have no static Fedora packages and stay shared.",
    "",
    "  -dbg        debug build (default: release)",
    "  -clean      delete the output directory first",
    "  -h | -help  this text",
  ];
  return lines.join("\n");
}

function parseArgs(action: FedoraAction, args: string[]): Options {
  const opts: Options = { debug: false, clean: false, runArgs: [] };
  for (let i = 0; i < args.length; i++) {
    const a = args[i]!;
    if (a === "--" && action === "run") {
      opts.runArgs = args.slice(i + 1);
      break;
    }
    if (a === "-dbg") opts.debug = true;
    else if (a === "-clean") opts.clean = true;
    else if (a === "-h" || a === "-help" || a === "--help") throw new CliError("");
    else if (action === "run") {
      // bun drops a leading "--", so the first argument that isn't ours starts the app's
      opts.runArgs = args.slice(i);
      break;
    } else throw new CliError(`unknown option: ${a}`);
  }
  return opts;
}

function bashScript(action: FedoraAction, opts: Options, rootDir: string): string {
  const cfg = opts.debug ? "dbg" : "rel";
  const buildArgs = [`-${cfg}`, "-static", ...(opts.clean ? ["-clean"] : [])];
  const exe = `out/${outPlat}/${cfg}/${defaultTarget}`;
  const dst = `${installDir}/${defaultTarget}`;
  const lines = [
    "set -euo pipefail",
    `step() { printf '\\n== %s\\n' "$*"; }`,
    `run() { printf '> %s\\n' "$*"; "$@"; }`,
    'SUDO=""',
    'if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi',
    "",
    'step "packages"',
    "missing=()",
    `for p in ${packages.join(" ")}; do`,
    '  rpm -q "$p" >/dev/null 2>&1 || missing+=("$p")',
    "done",
    'if [ "${#missing[@]}" -gt 0 ]; then',
    '  run $SUDO dnf install -y "${missing[@]}"',
    "else",
    `  echo "all ${packages.length} packages already installed"`,
    "fi",
    "",
    'step "bun"',
    'export PATH="$HOME/.bun/bin:$PATH"',
    "if ! command -v bun >/dev/null 2>&1; then",
    '  echo "> curl -fsSL https://bun.sh/install | bash"',
    "  curl -fsSL https://bun.sh/install | bash",
    "fi",
    "run bun --version",
    "",
    `run cd ${shellQuote(rootDir)}`,
    // a checkout on a Windows drive is owned by nobody git trusts
    'if ! git rev-parse HEAD >/dev/null 2>&1; then run git config --global --add safe.directory "$PWD"; fi',
    "",
    `step "build (${cfg})"`,
    `export ${linuxOutEnv}=${outPlat}`,
    `run bun cmd/ng-build.ts ${buildArgs.join(" ")}`,
    `run ls -l ${exe}`,
    `echo "> shared libraries ${exe} needs:"`,
    `ldd ${exe} | sed -e 's/^[[:space:]]*//' -e 's/ .*//' | sort | xargs echo " "`,
    "",
  ];
  if (action === "run") {
    lines.push('step "run"', `run ${exe} ${opts.runArgs.map(shellQuote).join(" ")}`);
  } else {
    lines.push(
      'step "install"',
      `run $SUDO install -D -m 755 ${exe} ${dst}`,
      `run ls -l ${dst}`,
      `echo "installed: run it with ${defaultTarget}"`,
    );
  }
  lines.push("");
  return lines.join("\n");
}

// "FedoraLinux-44": the newest installed Fedora distro
function findWslDistro(): string | null {
  const r = Bun.spawnSync(["wsl", "--list", "--quiet"], {
    stdout: "pipe",
    stderr: "pipe",
    env: { ...process.env, WSL_UTF8: "1" },
  });
  if (r.exitCode !== 0) return null;
  const names = r.stdout
    .toString()
    .split(/\r?\n/)
    .map((s) => s.trim())
    .filter((s) => s.startsWith(wslDistroPrefix));
  names.sort((a, b) => a.localeCompare(b, undefined, { numeric: true }));
  return names.at(-1) ?? null;
}

function isFedora(): boolean {
  try {
    return /^ID=fedora$/m.test(readFileSync("/etc/os-release", "utf8"));
  } catch {
    return false;
  }
}

// base64 so neither wsl.exe's argument handling nor bash mangles the script
function bashCmd(script: string): string[] {
  const b64 = Buffer.from(script, "utf8").toString("base64");
  return ["bash", "-lc", `echo ${b64} | base64 -d | bash`];
}

function fail(msg: string): never {
  console.error(msg);
  process.exit(1);
}

export async function fedoraMain(action: FedoraAction): Promise<never> {
  let opts: Options;
  try {
    opts = parseArgs(action, process.argv.slice(2));
  } catch (e) {
    if (!(e instanceof CliError)) throw e;
    if (e.message) console.error(`${e.message}\n`);
    console.log(usage(action));
    process.exit(e.message ? 1 : 0);
  }

  let cmd: string[];
  if (process.platform === "win32") {
    if (!Bun.which("wsl")) fail("wsl.exe not found. Install WSL, then: wsl --install FedoraLinux-44 --no-launch");
    const distro = findWslDistro();
    if (!distro) fail("No Fedora WSL distro found. Install one with: wsl --install FedoraLinux-44 --no-launch");
    console.log(`> wsl -d ${distro}`);
    cmd = ["wsl", "-d", distro, "-e", ...bashCmd(bashScript(action, opts, toWslPath(root)))];
  } else if (process.platform === "linux" && isFedora()) {
    cmd = bashCmd(bashScript(action, opts, root));
  } else {
    fail("This script needs Fedora, or Windows with a Fedora WSL distro.");
  }
  const p = Bun.spawn(cmd, { stdout: "inherit", stderr: "inherit", stdin: "inherit" });
  process.exit(await p.exited);
}
