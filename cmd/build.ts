import { copyFileSync, existsSync, readdirSync, statSync } from "node:fs";
import { cpus } from "node:os";
import { join, relative, resolve } from "node:path";
import { $ } from "bun";
import { clearDirPreserveSettings } from "./clean";
import { ensureNinja } from "./ninja";
import { detectVisualStudio2026, runLogged } from "./util";

type BuildMode = "windows" | "all" | "smoke" | "ci" | "daily" | "codeql" | "mingw" | "wine" | "build-no";
type Config = "debug" | "release";

interface BuildOptions {
  mode?: BuildMode;
  config?: Config;
  asan: boolean;
  clean: boolean;
  ninja: boolean;
  msbuild: boolean;
  win32: boolean;
  run: boolean;
  runArgs: string[];
  buildNo?: string;
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

MinGW cross-builds (they still produce a Windows exe):
  -mingw <-debug|-release> [-clean]
                           Direct MinGW cross-build on the current host
  -wine [-clean] [-run] [-- <SumatraPDF args>]
                           MinGW build on Linux and optionally run under Wine;
                           from Windows it runs through WSL Ubuntu

Other:
  -build-no [number|sha1] List recent build numbers or resolve a number or sha1
  -h | -help              Print this help

General options:
  -clean                  Clean the selected output directory first
  -ninja                  Use Ninja instead of MSBuild
  -msbuild                Use MSBuild (the default)
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
    ninja: false,
    msbuild: false,
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
    } else if (arg === "-ninja") {
      if (opts.ninja) throw new CliError("-ninja can only be specified once");
      opts.ninja = true;
    } else if (arg === "-msbuild") {
      if (opts.msbuild) throw new CliError("-msbuild can only be specified once");
      opts.msbuild = true;
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
    else if (arg === "-mingw") setMode(opts, "mingw");
    else if (arg === "-wine" || arg === "-win") setMode(opts, "wine");
    else if (arg === "-run") {
      if (opts.run) throw new CliError("-run can only be specified once");
      opts.run = true;
    } else if (arg === "-build-no") {
      setMode(opts, "build-no");
      const value = args[i + 1];
      if (value && !value.startsWith("-")) {
        opts.buildNo = value;
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
  reject(opts.clean && !["windows", "all", "mingw", "wine"].includes(mode), `-clean is not valid with -${mode}`);
  reject(opts.ninja && opts.msbuild, "-ninja and -msbuild cannot be used together");
  reject(opts.ninja && !["windows", "all", "smoke"].includes(mode), `-ninja is not valid with -${mode}`);
  reject(opts.msbuild && !["windows", "all", "smoke"].includes(mode), `-msbuild is not valid with -${mode}`);
  reject(opts.win32 && mode !== "windows", "-32 is only valid for Windows builds");
  reject(opts.run && mode !== "wine", "-run is only valid with -wine");
  reject(opts.runArgs.length > 0 && mode !== "wine", "arguments after -- are only valid with -wine");
  reject(opts.runArgs.length > 0 && !opts.run, "arguments after -- require -run");
}

function formatElapsed(ms: number): string {
  const seconds = Math.round(ms / 1000);
  const minutes = Math.floor(seconds / 60);
  if (minutes === 0) return `${seconds}s`;
  return `${minutes}m ${seconds % 60}s`;
}

async function buildApp(msbuildPath: string, configName: string, platform: string, target: string): Promise<void> {
  const targets = ["PdfFilter", "PdfPreview", "sumatrapdf-tool", target];
  for (const name of targets) {
    await runLogged(msbuildPath, [
      String.raw`vs2022\SumatraPDF.sln`,
      `/t:${name}`,
      `/p:Configuration=${configName};Platform=${platform}`,
      "/m",
    ]);
  }
}

async function buildWindows(config: Config, win32: boolean, clean: boolean, ninja: boolean): Promise<void> {
  const configName = config === "release" ? "Release" : "Debug";
  const platform = win32 ? "Win32" : "x64";
  const outDir = join("out", win32 ? "rel32" : config === "release" ? "rel64" : "dbg64");
  console.log(`${configName} ${platform} build`);
  if (clean) clearDirPreserveSettings(outDir);
  if (ninja) {
    await buildNinja([join("..", outDir, "SumatraPDF.exe")]);
  } else {
    const { msbuildPath } = detectVisualStudio2026();
    await buildApp(msbuildPath, configName, platform, "SumatraPDF");
  }
  printBinaries(outDir, new Set(["SumatraPDF.exe"]));
}

async function buildNinja(targets: string[]): Promise<void> {
  await ensureNinja();
  const jobs = Math.max(1, cpus().length - 1);
  await runLogged("ninja", ["-C", "ninja", "-j", `${jobs}`, ...targets]);
}

function printBinaries(dir: string, targets: Set<string>): void {
  const paths: string[] = [];
  const dynamicFiles = new Set([
    "SumatraPDF.exe",
    "libsumatrapdf.dll",
    "PdfFilter.dll",
    "PdfPreview.dll",
    "sumatrapdf-tool.exe",
    "test_util.exe",
  ]);
  const walk = (path: string): void => {
    for (const entry of readdirSync(path, { withFileTypes: true })) {
      const entryPath = join(path, entry.name);
      if (entry.isDirectory()) {
        walk(entryPath);
        continue;
      }
      const relPath = relative(dir, entryPath).replaceAll("\\", "/");
      const isDynamic = targets.has("SumatraPDF.exe") && dynamicFiles.has(relPath);
      if (entry.isFile() && (targets.has(entry.name) || isDynamic)) {
        paths.push(entryPath);
      }
    }
  };

  walk(dir);
  for (const path of paths.sort()) {
    const size = statSync(path).size;
    console.log(`${relative(".", path)}: ${(size / 1_000_000).toFixed(1)} MB, ${size.toLocaleString("en-US")}`);
  }
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

async function buildWindowsAsan(config: Config, clean: boolean, ninja: boolean): Promise<void> {
  const configName = config === "release" ? "Release" : "Debug";
  const outDir = join("out", config === "release" ? "rel64_asan" : "dbg64_asan");
  console.log(`${configName} ASan build (SumatraPDF-static.exe, x64_asan)`);
  if (clean) clearDirPreserveSettings(outDir);
  const { msbuildPath, vsRoot } = detectVisualStudio2026();
  if (ninja) {
    await buildNinja([join("..", outDir, "SumatraPDF-static.exe")]);
  } else {
    await runLogged(msbuildPath, [
      String.raw`vs2022\SumatraPDF.sln`,
      "/t:SumatraPDF-static",
      `/p:Configuration=${configName};Platform=x64_asan`,
      "/m",
    ]);
  }
  printBinaries(outDir, new Set(["SumatraPDF-static.exe"]));
  copyFileSync(findAsanDll(vsRoot), join(outDir, asanDllName));
  console.log(`exe: ${join(outDir, "SumatraPDF-static.exe")}`);
}

async function buildAll(clean: boolean, ninja: boolean): Promise<void> {
  const outDir = join("out", "rel64");
  console.log("Release x64 SumatraPDF and SumatraPDF-static build");
  if (clean) clearDirPreserveSettings(outDir);
  if (ninja) {
    await buildNinja([join("..", outDir, "SumatraPDF.exe"), join("..", outDir, "SumatraPDF-static.exe")]);
  } else {
    const { msbuildPath } = detectVisualStudio2026();
    await buildApp(msbuildPath, "Release", "x64", "SumatraPDF");
    await runLogged(msbuildPath, [
      String.raw`vs2022\SumatraPDF.sln`,
      "/t:SumatraPDF-static",
      "/p:Configuration=Release;Platform=x64",
      "/m",
    ]);
  }
  printBinaries(outDir, new Set(["SumatraPDF.exe", "SumatraPDF-static.exe"]));
}

async function buildSmoke(ninja: boolean): Promise<void> {
  const outDir = join("out", "rel64");
  console.log("smoke build");
  clearDirPreserveSettings(outDir);
  if (ninja) {
    await buildNinja([join("..", outDir, "SumatraPDF.exe"), join("..", outDir, "test_util.exe")]);
  } else {
    const { msbuildPath } = detectVisualStudio2026();
    await buildApp(msbuildPath, "Release", "x64", "SumatraPDF:Rebuild");
    await runLogged(msbuildPath, [
      String.raw`vs2022\SumatraPDF.sln`,
      String.raw`/t:tools\test_util:Rebuild`,
      "/p:Configuration=Release;Platform=x64",
      "/m",
    ]);
  }
  printBinaries(outDir, new Set(["SumatraPDF.exe", "test_util.exe"]));
  await runLogged(resolve(join(outDir, "test_util.exe")), [], outDir);
}

async function showBuildNo(query?: string): Promise<void> {
  const total = Number((await $`git rev-list --count HEAD`.text()).trim());
  if (!query) {
    const out = await $`git log -32 --oneline`.text();
    const lines = out.split("\n").filter((line) => line.trim() !== "");
    for (let i = 0; i < lines.length; i++) console.log(`${total - i + 1000} ${lines[i]}`);
    return;
  }
  if (/^\d+$/.test(query)) {
    const buildNo = Number(query);
    const skip = total - (buildNo - 1000);
    if (skip >= 0 && skip < total) {
      const line = (await $`git log -1 --skip ${skip} --oneline`.text()).trim();
      console.log(`${buildNo} ${line}`);
      return;
    }
  }
  const sha = (await $`git rev-parse --verify --quiet ${query}^{commit}`.nothrow().text()).trim();
  if (sha) {
    const count = Number((await $`git rev-list --count ${sha}`.text()).trim());
    const line = (await $`git log -1 --oneline ${sha}`.text()).trim();
    console.log(`${count + 1000} ${line}`);
    return;
  }
  if (/^\d+$/.test(query)) throw new Error(`build number ${query} is out of range`);
  throw new Error(`unknown commit or build number: ${query}`);
}

// the wine build runs in WSL Ubuntu when started from Windows
async function runWslLauncher(args: string[]): Promise<void> {
  const proc = Bun.spawn(["bun", "cmd/helper/wsl-build.ts", "-win", ...args], {
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const code = await proc.exited;
  if (code !== 0) throw new Error(`WSL wine build failed with exit code ${code}`);
}

async function runBuild(opts: BuildOptions): Promise<void> {
  const mode = opts.mode!;
  if (mode === "windows") {
    const config = opts.config ?? "debug";
    if (opts.asan) await buildWindowsAsan(config, opts.clean, opts.ninja);
    else await buildWindows(config, opts.win32, opts.clean, opts.ninja);
  } else if (mode === "all") await buildAll(opts.clean, opts.ninja);
  else if (mode === "smoke") await buildSmoke(opts.ninja);
  else if (mode === "ci") {
    const { buildCi } = await import("./helper/ci-build");
    await buildCi();
  } else if (mode === "daily") {
    const { buildDaily } = await import("./helper/daily-build");
    await buildDaily();
  } else if (mode === "codeql") {
    const { buildCodeql } = await import("./helper/codeql-build");
    await buildCodeql();
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
      await runWslLauncher(args);
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
  const timeStart = performance.now();
  try {
    await runBuild(opts);
  } finally {
    console.log(`build took ${formatElapsed(performance.now() - timeStart)}`);
  }
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nBuild failed: ${message}`);
  process.exitCode = 1;
}
