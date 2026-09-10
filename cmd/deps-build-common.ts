/**
 * Shared helpers for building SumatraPDF dependency static libraries
 * (used by the macOS, Linux, and MinGW build implementations).
 */

import { Glob } from "bun";
import { mkdirSync, existsSync, readFileSync, statSync, rmSync, writeFileSync } from "node:fs";
import { join, extname, dirname } from "node:path";
import { cpus } from "node:os";

export interface BuildTools {
  cc: string;
  cxx: string;
  ar: string;
}

export interface FileGroup {
  dir: string;
  patterns: string[];
}

export interface LibDef {
  name: string;
  files: FileGroup[];
  defines: string[];
  includes: string[];
  /** true = always -Os -DNDEBUG (optimized_conf); false = mixed_dbg_rel_conf */
  alwaysOptimize: boolean;
  debugExtraDefines?: string[];
  releaseExtraDefines?: string[];
  rtti?: boolean;
  exceptions?: boolean;
  extraCflags?: string[];
}

export interface BuildLibraryOptions {
  tools: BuildTools;
  commonDefines: string[];
  commonFlags?: string[];
  cxxFlags: string[];
  jobs: number;
}

export const DEFAULT_JOBS = Math.max(1, Math.min(4, cpus().length));

export function invalidateObjsIfBuildChanged(
  outDir: string,
  tools: BuildTools,
  config: string,
  commonFlags: string[],
): void {
  const stampPath = join(outDir, ".compiler");
  const stamp = `${tools.cc}\n${tools.cxx}\n${config}\n${commonFlags.join("\n")}\n`;
  let same = false;
  if (existsSync(stampPath)) {
    try {
      same = readFileSync(stampPath, "utf8") === stamp;
    } catch {}
  }
  if (!same && existsSync(join(outDir, "obj"))) {
    console.log("Compiler, config, or common flags changed; rebuilding objects...");
    rmSync(join(outDir, "obj"), { recursive: true, force: true });
  }
  mkdirSync(outDir, { recursive: true });
  writeFileSync(stampPath, stamp);
}

const kX86OnlyCflagRe = /^-m(no-)?(sse|avx|mmx|f16c|fma|aes|pclmul|popcnt|bmi|lzcnt|movbe|xop)/;

/**
 * Drops x86-only `-m` flags (`-msse4.1` and friends) when not building for x86.
 * clang *rejects* them on other targets ("unsupported option '-msse4.1' for
 * target 'arm64-apple-darwin'") instead of ignoring them, and the intrinsics
 * they enable already sit behind each library's own x86 `#ifdef`s, so there is
 * nothing to replace them with. Mutates lib.
 */
export function dropX86OnlyCflags(lib: LibDef, arch: string): void {
  if (arch === "x64" || arch === "x86" || !lib.extraCflags) {
    return;
  }
  lib.extraCflags = lib.extraCflags.filter((f) => !kX86OnlyCflagRe.test(f));
}

/** Resolve file patterns, returning only .c/.cpp/.cc source files */
export async function resolveSources(groups: FileGroup[]): Promise<string[]> {
  const compilableExts = new Set(["c", "cpp", "cc"]);
  const result = new Set<string>();
  for (const { dir, patterns } of groups) {
    for (const pat of patterns) {
      const fullPat = join(dir, pat);
      const glob = new Glob(fullPat);
      for await (const path of glob.scan(".")) {
        const ext = extname(path).slice(1).toLowerCase();
        if (compilableExts.has(ext)) {
          result.add(path);
        }
      }
    }
  }
  return [...result].sort();
}

/** Create a unique .o path from a source path */
export function objPath(outDir: string, libName: string, src: string): string {
  const flat = src.replace(/[\\/]/g, "__").replace(/\.[^.]+$/, ".o");
  return join(outDir, "obj", libName, flat);
}

/** Spawn a command; returns success/failure + captured streams */
export async function spawnCmd(
  args: string[],
  opts?: { cwd?: string; captureStdout?: boolean },
): Promise<{ ok: boolean; stderr: string; stdout: string }> {
  const proc = Bun.spawn(args, {
    stdout: opts?.captureStdout ? "pipe" : "ignore",
    stderr: "pipe",
    cwd: opts?.cwd,
  });
  const code = await proc.exited;
  const stderr = await new Response(proc.stderr).text();
  const stdout = opts?.captureStdout ? await new Response(proc.stdout).text() : "";
  return { ok: code === 0, stderr, stdout };
}

function depPathForObj(obj: string): string {
  return obj.replace(/\.o$/i, ".d");
}

function isUnderSrc(src: string): boolean {
  const n = src.replaceAll("\\", "/");
  return n.startsWith("src/") || n.includes("/src/");
}

// gcc/clang -MMD output: "foo.o: bar.cpp baz.h \\\n  qux.h"
function parseDepFile(depPath: string): string[] {
  let text: string;
  try {
    text = readFileSync(depPath, "utf8");
  } catch {
    return [];
  }
  const colon = text.indexOf(":");
  if (colon < 0) {
    return [];
  }
  return text
    .slice(colon + 1)
    .replace(/\\\r?\n/g, " ")
    .trim()
    .split(/\s+/)
    .filter(Boolean);
}

let cachedSrcHeaderMaxMtime: number | undefined;

async function srcHeaderMaxMtime(): Promise<number> {
  if (cachedSrcHeaderMaxMtime !== undefined) {
    return cachedSrcHeaderMaxMtime;
  }
  let max = 0;
  const glob = new Glob("src/**/*.{h,hh,hpp,inc}");
  for await (const path of glob.scan({ dot: false })) {
    try {
      const t = statSync(path).mtimeMs;
      if (t > max) {
        max = t;
      }
    } catch {}
  }
  cachedSrcHeaderMaxMtime = max;
  return max;
}

function newerThanObj(path: string, objMtime: number): boolean {
  try {
    return statSync(path).mtimeMs >= objMtime;
  } catch {
    return true;
  }
}

async function objectNeedsCompile(src: string, obj: string): Promise<boolean> {
  if (!existsSync(obj)) {
    return true;
  }
  let objMtime: number;
  try {
    objMtime = statSync(obj).mtimeMs;
  } catch {
    return true;
  }
  if (newerThanObj(src, objMtime)) {
    return true;
  }
  const depPath = depPathForObj(obj);
  if (existsSync(depPath)) {
    for (const dep of parseDepFile(depPath)) {
      if (newerThanObj(dep, objMtime)) {
        return true;
      }
    }
    return false;
  }
  // No .d yet (object from before header tracking). For src/ treat any
  // newer project header as a dep so a StrVec.h-sized layout change
  // rebuilds Dict_ut.o instead of linking a mixed ABI. ext/ stays
  // src-mtime-only until the next compile writes a .d.
  if (isUnderSrc(src) && (await srcHeaderMaxMtime()) >= objMtime) {
    return true;
  }
  return false;
}

function withDepArgs(args: string[], obj: string): string[] {
  if (args.includes("-MMD") || args.includes("-MD")) {
    return args;
  }
  return [...args, "-MMD", "-MP", "-MF", depPathForObj(obj)];
}

/** Compile a list of {src, obj, args} units in parallel */
export async function compileAll(units: { src: string; obj: string; args: string[] }[], jobs: number): Promise<void> {
  let idx = 0;
  let failed = 0;
  const total = units.length;

  async function worker() {
    while (true) {
      const i = idx++;
      if (i >= total) break;
      const u = units[i];
      process.stdout.write(`  [${i + 1}/${total}] ${u.src}\n`);
      mkdirSync(dirname(u.obj), { recursive: true });

      if (!(await objectNeedsCompile(u.src, u.obj))) {
        continue;
      }

      try {
        rmSync(u.obj);
      } catch {}
      const res = await spawnCmd(withDepArgs(u.args, u.obj));
      if (!res.ok) {
        console.error(`FAILED: ${u.src}`);
        if (res.stderr) console.error(res.stderr.trimEnd().slice(0, 1000));
        failed++;
        try {
          rmSync(u.obj);
        } catch {}
        try {
          rmSync(depPathForObj(u.obj));
        } catch {}
      }
      const done = i + 1;
      if (done % 100 === 0 || done === total) {
        process.stdout.write(`\r  [${done}/${total}]`);
      }
    }
  }

  await Promise.all(Array.from({ length: Math.min(jobs, total) }, () => worker()));
  if (total > 0) process.stdout.write("\n");
  if (failed > 0) {
    throw new Error(`${failed} file(s) failed to compile`);
  }
}

/** Create a static .a archive from object files */
export async function createArchive(tools: BuildTools, archivePath: string, objFiles: string[]): Promise<void> {
  if (objFiles.length === 0) return;
  mkdirSync(dirname(archivePath), { recursive: true });
  rmSync(archivePath, { force: true });
  const batch = 200;
  for (let i = 0; i < objFiles.length; i += batch) {
    const chunk = objFiles.slice(i, i + batch);
    const flag = i === 0 ? "rcs" : "rs";
    const res = await spawnCmd([tools.ar, flag, archivePath, ...chunk]);
    if (!res.ok) throw new Error(`ar failed: ${res.stderr}`);
  }
}

export async function buildLibrary(
  lib: LibDef,
  outDir: string,
  isRelease: boolean,
  opts: BuildLibraryOptions,
): Promise<{ archive: string; objs: string[] }> {
  const { tools, commonDefines, cxxFlags, jobs } = opts;
  const commonFlags = opts.commonFlags ?? [];
  console.log(`Building ${lib.name}...`);

  const sources = await resolveSources(lib.files);
  if (sources.length === 0) {
    console.log(`  (no sources found for ${lib.name})`);
    return { archive: "", objs: [] };
  }
  console.log(`  ${sources.length} source files`);

  mkdirSync(join(outDir, "obj", lib.name), { recursive: true });

  let optFlags: string[];
  let configDefines: string[];

  if (lib.alwaysOptimize) {
    optFlags = ["-Os"];
    configDefines = ["NDEBUG"];
  } else if (isRelease) {
    optFlags = ["-Os"];
    configDefines = ["NDEBUG"];
  } else {
    optFlags = ["-O0", "-g"];
    configDefines = ["DEBUG"];
  }

  const extraDefs = isRelease ? (lib.releaseExtraDefines ?? []) : (lib.debugExtraDefines ?? []);
  const allDefines = [...commonDefines, ...lib.defines, ...configDefines, ...extraDefs];
  const defineFlags = allDefines.map((d) => `-D${d}`);
  const includeFlags = lib.includes.map((d) => `-I${d}`);

  const units: { src: string; obj: string; args: string[] }[] = [];
  for (const src of sources) {
    const ext = extname(src).slice(1).toLowerCase();
    const isCpp = ext === "cpp" || ext === "cc";
    const compiler = isCpp ? tools.cxx : tools.cc;

    const langFlags: string[] = [];
    if (isCpp) {
      langFlags.push("-std=c++23", ...cxxFlags);
      if (!lib.rtti) langFlags.push("-fno-rtti");
      if (!lib.exceptions) langFlags.push("-fno-exceptions");
    }

    let warnFlags = ["-w"];
    if (!isCpp) {
      warnFlags = [
        "-w",
        "-Wno-incompatible-pointer-types",
        "-Wno-int-conversion",
        "-Wno-implicit-function-declaration",
      ];
    }

    const obj = objPath(outDir, lib.name, src);
    units.push({
      src,
      obj,
      args: [
        compiler,
        ...optFlags,
        ...defineFlags,
        ...includeFlags,
        ...commonFlags,
        ...warnFlags,
        ...langFlags,
        ...(lib.extraCflags ?? []),
        "-c",
        src,
        "-o",
        obj,
      ],
    });
  }

  await compileAll(units, jobs);

  const objs = units.map((u) => u.obj);
  const archivePath = join(outDir, "lib", `lib${lib.name}.a`);
  await createArchive(tools, archivePath, objs);
  console.log(`  -> ${archivePath}`);
  return { archive: archivePath, objs };
}
