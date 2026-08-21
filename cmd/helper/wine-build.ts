/**
 * Cross-compile SumatraPDF on Linux for running under Wine.
 *
 * Invoked by cmd/build.ts -wine.
 *
 * Requires: gcc-mingw-w64-x86-64, g++-mingw-w64-x86-64 (and wine for -run)
 */

import { existsSync, mkdirSync } from "node:fs";
import { cpus } from "node:os";
import { join } from "node:path";
import { clearDirPreserveSettings } from "../clean";
import { buildMingw, type MingwTools } from "./mingw-build";

const OUT_DIR = join("out", "dbg64-wine");
const EXE_PATH = join(OUT_DIR, "SumatraPDF.exe");

function logIfRunningUnderWine(): void {
  if (process.env.WINELOADER) {
    console.log("Note: this script is running under Wine");
  }
}

function requireLinux(): void {
  if (process.platform !== "linux") {
    throw new Error(`Wine build must be run on Linux (got ${process.platform})`);
  }
}

function resolveTool(role: string, candidates: string[]): string {
  for (const name of candidates) {
    const path = Bun.which(name);
    if (path) {
      return name;
    }
  }
  throw new Error(
    `Could not find ${role}. Install the mingw-w64 toolchain: ` +
      "sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64",
  );
}

function resolveMingwTools(): MingwTools {
  const prefix = "x86_64-w64-mingw32";
  // Prefer the posix-threads variants: Debian's default *-gcc/*-g++ are the
  // win32-threads flavor whose libstdc++ lacks std::mutex/std::thread (needed
  // by some C++ deps); on Ubuntu the plain names are already the posix flavor.
  return {
    cc: resolveTool("mingw gcc", [`${prefix}-gcc-posix`, `${prefix}-gcc`, "gcc-mingw-w64-x86-64"]),
    cxx: resolveTool("mingw g++", [`${prefix}-g++-posix`, `${prefix}-g++`, "g++-mingw-w64-x86-64"]),
    ar: resolveTool("mingw ar", [`${prefix}-ar`, "ar-mingw-w64-x86-64"]),
    windres: resolveTool("mingw windres", [`${prefix}-windres`, "windres-mingw-w64-x86-64"]),
    objcopy: resolveTool("mingw objcopy", [`${prefix}-objcopy`, "objcopy-mingw-w64-x86-64"]),
  };
}

async function runUnderWine(extraArgs: string[]): Promise<void> {
  const wine = Bun.which("wine");
  if (!wine) {
    throw new Error("wine not found in PATH (required for -run)");
  }
  if (!existsSync(EXE_PATH)) {
    throw new Error(`Executable not found: ${EXE_PATH}`);
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
  if (code !== 0) {
    throw new Error(`Wine exited with code ${code}`);
  }
}

export interface WineBuildOptions {
  clean?: boolean;
  run?: boolean;
  runArgs?: string[];
}

export async function buildWine(opts: WineBuildOptions): Promise<void> {
  requireLinux();
  logIfRunningUnderWine();

  const doClean = opts.clean ?? false;
  const doRun = opts.run ?? false;
  const runArgs = opts.runArgs ?? [];
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
