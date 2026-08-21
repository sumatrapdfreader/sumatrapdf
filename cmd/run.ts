import { spawn } from "node:child_process";
import { join } from "node:path";

type Config = "debug" | "release";

interface RunOptions {
  config: Config;
  asan: boolean;
  clean: boolean;
}

const usage = `Usage: bun cmd/run.ts <-dbg | -rel> [-asan] [-clean]

Build and run SumatraPDF.

  -dbg     debug build
  -rel     release build
  -asan    enable AddressSanitizer in the selected build
  -clean   build everything from scratch`;

class CliError extends Error {}

function parseArgs(args: string[]): RunOptions | undefined {
  if (args.length === 0) return undefined;

  let config: Config | undefined;
  let asan = false;
  let clean = false;
  for (const arg of args) {
    if (arg === "-dbg" || arg === "-rel") {
      const next = arg === "-dbg" ? "debug" : "release";
      if (config) throw new CliError(`-${config === "debug" ? "dbg" : "rel"} and ${arg} cannot be combined`);
      config = next;
    } else if (arg === "-asan") {
      if (asan) throw new CliError("-asan can only be specified once");
      asan = true;
    } else if (arg === "-clean") {
      if (clean) throw new CliError("-clean can only be specified once");
      clean = true;
    } else {
      throw new CliError(`unknown option: ${arg}`);
    }
  }
  if (!config) throw new CliError("one of -dbg or -rel is required");
  return { config, asan, clean };
}

async function run(
  command: string[],
  description: string,
  env: Record<string, string | undefined> = process.env,
): Promise<void> {
  console.log(`> ${command.join(" ")}`);
  const proc = Bun.spawn(command, {
    cwd: ".",
    env,
    stdin: "inherit",
    stdout: "inherit",
    stderr: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) throw new Error(`${description} failed with exit code ${exitCode}`);
}

async function build(opts: RunOptions): Promise<void> {
  const configFlag = opts.config === "release" ? "-release" : "-debug";
  if (process.platform !== "win32") throw new Error(`unsupported operating system: ${process.platform}`);
  const args = ["bun", "cmd/build.ts"];
  args.push(configFlag);
  if (opts.asan) args.push("-asan");
  if (opts.clean) args.push("-clean");
  await run(args, "build");
}

function runWindows(opts: RunOptions): void {
  const dir = opts.config === "release" ? "rel64" : "dbg64";
  const outDir = opts.asan ? `${dir}_asan` : dir;
  const exe = join("out", outDir, opts.asan ? "SumatraPDF-static.exe" : "SumatraPDF.exe");
  const proc = spawn(exe, ["-for-testing"], { cwd: ".", detached: true, stdio: "ignore" });
  proc.unref();
}

function runApp(opts: RunOptions): void {
  runWindows(opts);
}

async function main(): Promise<void> {
  let opts: RunOptions | undefined;
  try {
    opts = parseArgs(Bun.argv.slice(2));
  } catch (error) {
    if (!(error instanceof CliError)) throw error;
    console.error(`error: ${error.message}\n`);
    console.error(usage);
    process.exitCode = 1;
    return;
  }
  if (!opts) {
    console.log(usage);
    return;
  }
  await build(opts);
  runApp(opts);
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nRun failed: ${message}`);
  process.exitCode = 1;
}
