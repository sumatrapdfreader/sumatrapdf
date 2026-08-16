import { existsSync } from "node:fs";
import { join } from "node:path";

const WSL_DISTRO = "Ubuntu";

type Variant = "debug" | "release" | "asan";

const usage = `Usage: bun cmd/wsl-run.ts <-dbg | -rel | -asan>

Build and run the GTK4 SumatraPDF application under WSL Ubuntu.

  -dbg     debug build (out/linux-dbg64/SumatraPDF)
  -rel     release build (out/linux-rel64/SumatraPDF)
  -asan    AddressSanitizer build (out/linux-asan64/SumatraPDF)`;

class CliError extends Error {}

function parseArgs(args: string[]): Variant | undefined {
  if (args.length === 0) {
    return undefined;
  }

  let variant: Variant | undefined;
  for (const arg of args) {
    let next: Variant;
    if (arg === "-dbg") {
      next = "debug";
    } else if (arg === "-rel") {
      next = "release";
    } else if (arg === "-asan") {
      next = "asan";
    } else {
      throw new CliError(`unknown option: ${arg}`);
    }

    if (variant) {
      throw new CliError(
        `-${variant === "debug" ? "dbg" : variant === "release" ? "rel" : "asan"} and ${arg} cannot be combined`,
      );
    }
    variant = next;
  }
  return variant;
}

function shellQuote(s: string): string {
  return `'${s.replace(/'/g, `'\\''`)}'`;
}

function windowsPathToWsl(path: string): string {
  const match = /^([A-Za-z]):[\\/](.*)$/.exec(path);
  if (!match) {
    return path.replace(/\\/g, "/");
  }
  return `/mnt/${match[1].toLowerCase()}/${match[2].replace(/\\/g, "/")}`;
}

async function run(
  command: string[],
  description: string,
  displayCommand?: string,
  env: Record<string, string | undefined> = process.env,
): Promise<void> {
  console.log(`> ${displayCommand ?? command.join(" ")}`);
  const proc = Bun.spawn(command, {
    env,
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`${description} failed with exit code ${exitCode}`);
  }
}

async function runLinuxApp(exePath: string, asan: boolean): Promise<void> {
  if (process.platform === "linux") {
    const current = process.env.ASAN_OPTIONS;
    const symbolizer = Bun.which("llvm-symbolizer");
    const symbolizerOptions =
      symbolizer && !symbolizer.toLowerCase().endsWith(".exe")
        ? [`external_symbolizer_path=${symbolizer}`]
        : ["allow_addr2line=1", "external_symbolizer_path=/usr/bin/addr2line"];
    const env = asan
      ? {
          ...process.env,
          ASAN_OPTIONS: [...(current ? [current] : []), "symbolize=1", ...symbolizerOptions].join(":"),
        }
      : process.env;
    await run([exePath], "SumatraPDF", undefined, env);
    return;
  }
  if (process.platform !== "win32") {
    throw new Error("wsl-run.ts must be run on Windows or Linux under WSL");
  }
  if (!Bun.which("wsl")) {
    throw new Error("wsl not found in PATH. Install WSL and the Ubuntu distro.");
  }

  const wslCwd = windowsPathToWsl(process.cwd());
  const asanSetup = [
    'symbolizer="$(command -v llvm-symbolizer || true)"',
    'if [ -n "$symbolizer" ]; then',
    '  export ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}symbolize=1:external_symbolizer_path=$symbolizer"',
    "else",
    '  export ASAN_OPTIONS="${ASAN_OPTIONS:+${ASAN_OPTIONS}:}symbolize=1:allow_addr2line=1:external_symbolizer_path=/usr/bin/addr2line"',
    "fi",
  ];
  const remoteScript = [
    "set -euo pipefail",
    `cd ${shellQuote(wslCwd)}`,
    ...(asan ? asanSetup : []),
    `exec ${shellQuote(exePath)}`,
    "",
  ].join("\n");
  const encodedScript = Buffer.from(remoteScript, "utf8").toString("base64");
  const wrapper = `echo ${encodedScript} | base64 -d | bash -l`;
  await run(["wsl", "-d", WSL_DISTRO, "-e", "bash", "-lc", wrapper], "SumatraPDF", `wsl -d ${WSL_DISTRO}: ${exePath}`);
}

async function main(): Promise<void> {
  let variant: Variant | undefined;
  try {
    variant = parseArgs(Bun.argv.slice(2));
  } catch (error) {
    if (!(error instanceof CliError)) {
      throw error;
    }
    console.error(`error: ${error.message}\n`);
    console.error(usage);
    process.exitCode = 1;
    return;
  }

  if (!variant) {
    console.log(usage);
    return;
  }
  if (!existsSync(join("cmd", "build.ts"))) {
    throw new Error("Run this command from the SumatraPDF repository root.");
  }

  const buildFlag = variant === "debug" ? "-debug" : variant === "release" ? "-release" : "-asan";
  const outDir = variant === "debug" ? "linux-dbg64" : variant === "release" ? "linux-rel64" : "linux-asan64";
  await run(["bun", "cmd/build.ts", "-linux", buildFlag], "Linux build");
  await runLinuxApp(`./out/${outDir}/SumatraPDF`, variant === "asan");
}

if (import.meta.main) {
  main().catch((error: unknown) => {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`error: ${message}`);
    process.exit(1);
  });
}
