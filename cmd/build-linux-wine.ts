/**
 * Cross-compile SumatraPDF on Linux for running under Wine.
 *
 * Usage:
 *   bun bun/build-linux-wine.ts          # debug static build -> out/dbg64-wine/SumatraPDF.exe
 *   bun bun/build-linux-wine.ts -clean   # clean out/dbg64-wine (preserve settings)
 *   bun bun/build-linux-wine.ts -run     # build, then run under Wine
 *   bun bun/build-linux-wine.ts -clean -run
 *
 * Requires: gcc-mingw-w64-x86-64, g++-mingw-w64-x86-64 (and wine for -run)
 */

import { existsSync, mkdirSync } from "node:fs";
import { cpus } from "node:os";
import { join } from "node:path";
import { clearDirPreserveSettings } from "../cmd/clean";
import { buildMingw, type MingwTools } from "../cmd/build-with-mingw";

const OUT_DIR = join("out", "dbg64-wine");
const EXE_PATH = join(OUT_DIR, "SumatraPDF.exe");

function logIfRunningUnderWine(): void {
  if (process.env.WINELOADER) {
    console.log("Note: this script is running under Wine");
  }
}

function requireLinux(): void {
  if (process.platform !== "linux") {
    console.error(`This script must be run on Linux (got ${process.platform})`);
    process.exit(1);
  }
}

function resolveTool(role: string, candidates: string[]): string {
  for (const name of candidates) {
    const path = Bun.which(name);
    if (path) {
      return name;
    }
  }
  console.error(`Could not find ${role}.`);
  console.error("Install the mingw-w64 toolchain, e.g.:");
  console.error("  sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64");
  process.exit(1);
}

function resolveMingwTools(): MingwTools {
  const prefix = "x86_64-w64-mingw32";
  // Prefer the posix-threads variants: Debian's default *-gcc/*-g++ are the
  // win32-threads flavor whose libstdc++ lacks std::mutex/std::thread (needed
  // by libheif); on Ubuntu the plain names are already the posix flavor.
  return {
    cc: resolveTool("mingw gcc", [`${prefix}-gcc-posix`, `${prefix}-gcc`, "gcc-mingw-w64-x86-64"]),
    cxx: resolveTool("mingw g++", [`${prefix}-g++-posix`, `${prefix}-g++`, "g++-mingw-w64-x86-64"]),
    ar: resolveTool("mingw ar", [`${prefix}-ar`, "ar-mingw-w64-x86-64"]),
    windres: resolveTool("mingw windres", [`${prefix}-windres`, "windres-mingw-w64-x86-64"]),
    objcopy: resolveTool("mingw objcopy", [`${prefix}-objcopy`, "objcopy-mingw-w64-x86-64"]),
  };
}

function parseArgs(argv: string[]): { doClean: boolean; doRun: boolean; runArgs: string[] } {
  let doClean = false;
  let doRun = false;
  const runArgs: string[] = [];
  let i = 0;
  while (i < argv.length) {
    const arg = argv[i];
    if (arg === "-clean") {
      doClean = true;
    } else if (arg === "-run") {
      doRun = true;
    } else if (arg === "--") {
      runArgs.push(...argv.slice(i + 1));
      break;
    } else {
      console.error(`Unknown argument: ${arg}`);
      console.error("Usage: bun bun/build-linux-wine.ts [-clean] [-run] [-- extra exe args...]");
      process.exit(1);
    }
    i++;
  }
  return { doClean, doRun, runArgs };
}

async function runUnderWine(extraArgs: string[]): Promise<void> {
  const wine = Bun.which("wine");
  if (!wine) {
    console.error("wine not found in PATH (required for -run)");
    process.exit(1);
  }
  if (!existsSync(EXE_PATH)) {
    console.error(`Executable not found: ${EXE_PATH}`);
    process.exit(1);
  }

  const args = [EXE_PATH, "-for-testing", ...extraArgs];
  console.log(`Running: wine ${args.join(" ")}`);
  const proc = Bun.spawn(["wine", ...args], {
    cwd: process.cwd(),
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const code = await proc.exited;
  process.exit(code);
}

async function main(): Promise<void> {
  requireLinux();
  logIfRunningUnderWine();

  const { doClean, doRun, runArgs } = parseArgs(Bun.argv.slice(2));
  const onlyClean = doClean && !doRun;

  if (doClean) {
    mkdirSync(OUT_DIR, { recursive: true });
    console.log(`Cleaning ${OUT_DIR} (preserving settings)...`);
    clearDirPreserveSettings(OUT_DIR);
    if (onlyClean) {
      return;
    }
  }

  const tools = resolveMingwTools();
  const jobs = Math.max(1, cpus().length);
  console.log(`Using ${tools.cxx} / ${tools.cc} (${jobs} parallel jobs)`);

  await buildMingw({
    outDir: OUT_DIR,
    isRelease: false,
    tools,
    jobs,
  });

  if (doRun) {
    await runUnderWine(runArgs);
  }
}

if (import.meta.main) {
  main().catch((e: unknown) => {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`\nBuild failed: ${msg}`);
    process.exit(1);
  });
}