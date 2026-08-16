import { copyFileSync, existsSync, readdirSync } from "node:fs";
import { cpus } from "node:os";
import { join, resolve } from "node:path";
import { $ } from "bun";
import { clearDirPreserveSettings } from "./clean";
import { detectVisualStudio2026, runLogged } from "./util";

type BuildMode =
  | "windows"
  | "all"
  | "smoke"
  | "ci"
  | "daily"
  | "codeql"
  | "linux"
  | "mac"
  | "mac-remote"
  | "mingw"
  | "wine"
  | "build-no";
type Config = "debug" | "release";

interface BuildOptions {
  mode?: BuildMode;
  config?: Config;
  asan: boolean;
  clean: boolean;
  win32: boolean;
  run: boolean;
  runArgs: string[];
  branch?: string;
  buildNo?: number;
}

const usage = `Usage: bun cmd/build.ts <mode> [options]

Windows builds:
  -debug | -release       Build SumatraPDF.exe for x64
  -release -32            Build the 32-bit release
  -asan [-debug|-release] Build SumatraPDF-static.exe with MSVC ASan
  -all [-clean]           Build release SumatraPDF and SumatraPDF-static
  -smoke                  Rebuild release SumatraPDF and test_util, then run test_util
  -ci                     Build CI/pre-release artifacts
  -daily                  Build daily artifacts
  -codeql                 Build the static release target for CodeQL

Portable and cross-platform builds:
  -linux [-debug|-release] [-asan] [-clean]
                           Native Linux build; defaults to -asan
  -mac [-debug|-release] [-asan] [-clean]
                           Native macOS build; defaults to -debug
  -mac-remote -branch <name> [-debug|-release] [-asan] [-clean]
                           Build a temporary branch on the remote Mac
  -mingw <-debug|-release> [-clean]
                           Direct MinGW cross-build on the current host
  -wine [-clean] [-run] [-- <SumatraPDF args>]
                           MinGW build on Linux and optionally run under Wine;
                           from Windows, -linux and -wine run through WSL Ubuntu

Other:
  -build-no [number]      List recent build numbers or resolve one number
  -h | -help              Print this help

General options:
  -clean                  Clean the selected output directory first
  -32                     Select Win32 (valid only with Windows -release)`;

class CliError extends Error {}

function setMode(opts: BuildOptions, mode: BuildMode): void {
  if (opts.mode) {
    throw new CliError(`build modes -${opts.mode} and -${mode} cannot be used together`);
  }
  opts.mode = mode;
}

function setConfig(opts: BuildOptions, config: Config): void {
  if (opts.config) {
    throw new CliError(`-${opts.config} and -${config} cannot be used together`);
  }
  opts.config = config;
}

function parseArgs(args: string[]): BuildOptions | undefined {
  if (args.length === 0) {
    return undefined;
  }
  const helpArgs = new Set(["-h", "-help", "--help"]);
  if (args.some((arg) => helpArgs.has(arg))) {
    if (args.length !== 1) throw new CliError("help cannot be combined with other options");
    return undefined;
  }
  const opts: BuildOptions = {
    asan: false,
    clean: false,
    win32: false,
    run: false,
    runArgs: [],
  };

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "--") {
      opts.runArgs.push(...args.slice(i + 1));
      break;
    }
    if (arg === "-debug") setConfig(opts, "debug");
    else if (arg === "-release") setConfig(opts, "release");
    else if (arg === "-asan") {
      if (opts.asan) throw new CliError("-asan can only be specified once");
      opts.asan = true;
    } else if (arg === "-clean") {
      if (opts.clean) throw new CliError("-clean can only be specified once");
      opts.clean = true;
    } else if (arg === "-32") {
      if (opts.win32) throw new CliError("-32 can only be specified once");
      opts.win32 = true;
    } else if (arg === "-rel-32") {
      if (opts.win32) throw new CliError("-32 can only be specified once");
      setConfig(opts, "release");
      opts.win32 = true;
    } else if (arg === "-all") setMode(opts, "all");
    else if (arg === "-smoke") setMode(opts, "smoke");
    else if (arg === "-ci") setMode(opts, "ci");
    else if (arg === "-daily") setMode(opts, "daily");
    else if (arg === "-codeql") setMode(opts, "codeql");
    else if (arg === "-linux") setMode(opts, "linux");
    else if (arg === "-mac") setMode(opts, "mac");
    else if (arg === "-mac-remote") setMode(opts, "mac-remote");
    else if (arg === "-mingw") setMode(opts, "mingw");
    else if (arg === "-wine" || arg === "-win") setMode(opts, "wine");
    else if (arg === "-run") {
      if (opts.run) throw new CliError("-run can only be specified once");
      opts.run = true;
    } else if (arg === "-branch") {
      if (opts.branch) throw new CliError("-branch can only be specified once");
      const branch = args[++i];
      if (!branch || branch.startsWith("-")) throw new CliError("-branch requires a branch name");
      opts.branch = branch;
    } else if (arg === "-build-no") {
      setMode(opts, "build-no");
      const value = args[i + 1];
      if (value && !value.startsWith("-")) {
        if (!/^\d+$/.test(value)) throw new CliError(`invalid build number: ${value}`);
        opts.buildNo = Number(value);
        i++;
      }
    } else {
      throw new CliError(`unknown option: ${arg}`);
    }
  }

  if (!opts.mode) {
    if (opts.config || opts.asan || opts.win32) opts.mode = "windows";
    else throw new CliError("missing build mode");
  }
  validateOptions(opts);
  return opts;
}

function reject(condition: boolean, message: string): void {
  if (condition) throw new CliError(message);
}

function validateOptions(opts: BuildOptions): void {
  const mode = opts.mode!;
  const fixedModes: BuildMode[] = ["all", "smoke", "ci", "daily", "codeql", "wine", "build-no"];
  if (fixedModes.includes(mode)) {
    reject(!!opts.config, `-${opts.config} is not valid with -${mode}`);
    reject(opts.asan, `-asan is not valid with -${mode}`);
  }
  if (mode === "windows") {
    reject(!opts.config && !opts.asan, "Windows builds require -debug, -release, or -asan");
    reject(opts.win32 && (opts.config !== "release" || opts.asan), "-32 requires a non-ASan -release build");
  }
  if (mode === "mingw") {
    reject(!opts.config, "-mingw requires -debug or -release");
    reject(opts.asan, "-asan is not supported with -mingw");
  }
  reject(
    opts.clean && !["windows", "all", "linux", "mac", "mac-remote", "mingw", "wine"].includes(mode),
    `-clean is not valid with -${mode}`,
  );
  reject(opts.win32 && mode !== "windows", "-32 is only valid for Windows builds");
  reject(opts.run && mode !== "wine", "-run is only valid with -wine");
  reject(opts.runArgs.length > 0 && mode !== "wine", "arguments after -- are only valid with -wine");
  reject(opts.runArgs.length > 0 && !opts.run, "arguments after -- require -run");
  reject(!!opts.branch && mode !== "mac-remote", "-branch is only valid with -mac-remote");
  reject(mode === "mac-remote" && !opts.branch, "-mac-remote requires -branch <name>");
}

async function buildWindows(config: Config, win32: boolean, clean: boolean): Promise<void> {
  const configName = config === "release" ? "Release" : "Debug";
  const platform = win32 ? "Win32" : "x64";
  const outDir = join("out", win32 ? "rel32" : config === "release" ? "rel64" : "dbg64");
  const timeStart = performance.now();
  console.log(`${configName} ${platform} build`);
  if (clean) clearDirPreserveSettings(outDir);
  const { msbuildPath } = detectVisualStudio2026();
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    "/t:SumatraPDF",
    `/p:Configuration=${configName};Platform=${platform}`,
    "/m",
  ]);
  console.log(`build took ${((performance.now() - timeStart) / 1000).toFixed(1)}s`);
}

const asanDllName = "clang_rt.asan_dynamic-x86_64.dll";

function findAsanDll(vsRoot: string): string {
  const candidates = [join(vsRoot, String.raw`VC\Tools\MSVC`), join(vsRoot, String.raw`VC\Tools\Llvm\x64\lib\clang`)];
  const walk = (dir: string): string | undefined => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        const found = walk(path);
        if (found) return found;
      } else if (entry.name === asanDllName) {
        return path;
      }
    }
    return undefined;
  };
  for (const base of candidates) {
    if (existsSync(base)) {
      const found = walk(base);
      if (found) return found;
    }
  }
  throw new Error(`could not find ${asanDllName} under ${vsRoot}`);
}

async function buildWindowsAsan(config: Config, clean: boolean): Promise<void> {
  const configName = config === "release" ? "Release" : "Debug";
  const outDir = join("out", config === "release" ? "rel64_asan" : "dbg64_asan");
  const timeStart = performance.now();
  console.log(`${configName} ASan build (SumatraPDF-static.exe, x64_asan)`);
  if (clean) clearDirPreserveSettings(outDir);
  await runLogged(join("bin", "premake5.exe"), ["vs2022"]);
  const { msbuildPath, vsRoot } = detectVisualStudio2026();
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    "/t:SumatraPDF-static",
    `/p:Configuration=${configName};Platform=x64_asan`,
    "/m",
  ]);
  copyFileSync(findAsanDll(vsRoot), join(outDir, asanDllName));
  console.log(`build took ${((performance.now() - timeStart) / 1000).toFixed(1)}s`);
  console.log(`exe: ${join(outDir, "SumatraPDF-static.exe")}`);
}

async function buildAll(clean: boolean): Promise<void> {
  const outDir = join("out", "rel64");
  const timeStart = performance.now();
  console.log("Release x64 SumatraPDF and SumatraPDF-static build");
  if (clean) clearDirPreserveSettings(outDir);
  const { msbuildPath } = detectVisualStudio2026();
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    "/t:SumatraPDF;SumatraPDF-static",
    "/p:Configuration=Release;Platform=x64",
    "/m",
  ]);
  console.log(`build took ${((performance.now() - timeStart) / 1000).toFixed(1)}s`);
}

async function buildSmoke(): Promise<void> {
  const outDir = join("out", "rel64");
  const timeStart = performance.now();
  console.log("smoke build");
  clearDirPreserveSettings(outDir);
  const { msbuildPath } = detectVisualStudio2026();
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    String.raw`/t:SumatraPDF:Rebuild;tools\test_util:Rebuild`,
    "/p:Configuration=Release;Platform=x64",
    "/m",
  ]);
  await runLogged(resolve(join(outDir, "test_util.exe")), [], outDir);
  console.log(`smoke build took ${((performance.now() - timeStart) / 1000).toFixed(1)}s`);
}

async function showBuildNo(buildNo?: number): Promise<void> {
  const out = await $`git log --oneline`.text();
  const lines = out.split("\n").filter((line) => line.trim() !== "");
  const numberAt = (i: number) => lines.length - i + 1000;
  if (!buildNo) {
    for (let i = 0; i < Math.min(32, lines.length); i++) console.log(`${numberAt(i)} ${lines[i]}`);
    return;
  }
  const index = lines.length - (buildNo - 1000);
  if (index < 0 || index >= lines.length) throw new Error(`build number ${buildNo} is out of range`);
  console.log(`${buildNo} ${lines[index]}`);
}

async function runWslLauncher(args: string[], target: "linux" | "wine"): Promise<void> {
  const selector = target === "linux" ? "-linux" : "-win";
  const proc = Bun.spawn(["bun", "cmd/helper/wsl-build.ts", selector, ...args], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const code = await proc.exited;
  if (code !== 0) throw new Error(`WSL ${target} build failed with exit code ${code}`);
}

interface PortableConfig {
  isRelease: boolean;
  asan: boolean;
}

function portableConfig(opts: BuildOptions, defaultConfig: "debug" | "asan"): PortableConfig {
  const asan = opts.asan || (!opts.config && defaultConfig === "asan");
  const isRelease = opts.config === "release";
  return { isRelease, asan };
}

function portableOutDir(platform: "linux" | "mac", config: PortableConfig): string {
  if (config.asan) return `out/${platform}-${config.isRelease ? "rel64_asan" : "asan64"}`;
  return `out/${platform}-${config.isRelease ? "rel" : "dbg"}64`;
}

async function runBuild(opts: BuildOptions): Promise<void> {
  const mode = opts.mode!;
  if (mode === "windows") {
    const config = opts.config ?? "debug";
    if (opts.asan) await buildWindowsAsan(config, opts.clean);
    else await buildWindows(config, opts.win32, opts.clean);
  } else if (mode === "all") await buildAll(opts.clean);
  else if (mode === "smoke") await buildSmoke();
  else if (mode === "ci") {
    const { buildCi } = await import("./helper/ci-build");
    await buildCi();
  } else if (mode === "daily") {
    const { buildDaily } = await import("./helper/daily-build");
    await buildDaily();
  } else if (mode === "codeql") {
    const { buildCodeql } = await import("./helper/codeql-build");
    await buildCodeql();
  } else if (mode === "linux") {
    const config = portableConfig(opts, "asan");
    if (process.platform === "win32") {
      const flags = [config.isRelease ? "-release" : "-debug", ...(config.asan ? ["-asan"] : [])];
      await runWslLauncher([...flags, ...(opts.clean ? ["-clean"] : [])], "linux");
    } else {
      const { buildLinux } = await import("./helper/linux-build");
      await buildLinux({
        outDir: portableOutDir("linux", config),
        isRelease: config.isRelease,
        asan: config.asan,
        clean: opts.clean,
        jobs: Math.max(1, Math.min(4, cpus().length)),
      });
    }
  } else if (mode === "mac") {
    const config = portableConfig(opts, "debug");
    const { buildMac } = await import("./helper/mac-build");
    await buildMac({
      outDir: portableOutDir("mac", config),
      isRelease: config.isRelease,
      asan: config.asan,
      clean: opts.clean,
      jobs: Math.max(1, Math.min(4, cpus().length)),
    });
  } else if (mode === "mac-remote") {
    const config = portableConfig(opts, "debug");
    const { buildMacRemote } = await import("./helper/mac-remote-build");
    const flags = [config.isRelease ? "-release" : "-debug", ...(config.asan ? ["-asan"] : [])];
    await buildMacRemote(opts.branch!, [...flags, ...(opts.clean ? ["-clean"] : [])]);
  } else if (mode === "mingw") {
    const { buildMingw } = await import("./helper/mingw-build");
    await buildMingw({
      outDir: `out/mingw-${opts.config === "release" ? "rel" : "dbg"}64`,
      isRelease: opts.config === "release",
      clean: opts.clean,
    });
  } else if (mode === "wine") {
    if (process.platform === "win32") {
      const args = [...(opts.clean ? ["-clean"] : []), ...(opts.run ? ["-run"] : [])];
      if (opts.runArgs.length) args.push("--", ...opts.runArgs);
      await runWslLauncher(args, "wine");
    } else {
      const { buildWine } = await import("./helper/wine-build");
      await buildWine({ clean: opts.clean, run: opts.run, runArgs: opts.runArgs });
    }
  } else if (mode === "build-no") await showBuildNo(opts.buildNo);
}

async function main(): Promise<void> {
  let opts: BuildOptions | undefined;
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
  await runBuild(opts);
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nBuild failed: ${message}`);
  process.exitCode = 1;
}
