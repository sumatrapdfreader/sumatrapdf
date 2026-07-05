/**
 * Shared helpers for building SumatraPDF dependency static libraries
 * (used by cmd/build-mac.ts and cmd/build-with-mingw.ts).
 */

import { Glob } from "bun";
import { mkdirSync, existsSync, statSync, rmSync } from "node:fs";
import { readFile, writeFile } from "node:fs/promises";
import { join, extname, dirname } from "node:path";
import { cpus } from "node:os";

export interface BuildTools {
  cc: string;
  cxx: string;
  ar: string;
  /** PE objcopy (mingw) or Mach-O ld (mac) for font/binary embedding */
  embed: string;
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
  cxxFlags: string[];
  jobs: number;
  /** extra object files to add to the archive (e.g. embedded fonts) */
  extraObjs?: string[];
}

export const DEFAULT_JOBS = Math.max(1, Math.min(4, cpus().length));

/** Font files embedded into the mupdf static lib (premake fonts()). */
export const FONT_FILES = [
  { path: "mupdf/resources/fonts/urw/Dingbats.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/StandardSymbolsPS.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/droid/DroidSansFallbackFull.ttf", ext: "ttf" },
  { path: "mupdf/resources/fonts/sil/CharisSIL.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/noto/NotoSans-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansMath-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansSymbols-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansSymbols2-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoEmoji-Regular.ttf", ext: "ttf" },
  { path: "mupdf/resources/fonts/noto/NotoMusic-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSerif-Regular.otf", ext: "otf" },
];

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

/** Compile a list of {src, obj, args} units in parallel */
export async function compileAll(
  units: { src: string; obj: string; args: string[] }[],
  jobs: number,
): Promise<void> {
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

      if (existsSync(u.obj)) {
        try {
          const srcMtime = statSync(u.src).mtimeMs;
          const objMtime = statSync(u.obj).mtimeMs;
          if (objMtime > srcMtime) {
            continue;
          }
        } catch {}
      }

      try {
        rmSync(u.obj);
      } catch {}
      const res = await spawnCmd(u.args);
      if (!res.ok) {
        console.error(`FAILED: ${u.src}`);
        if (res.stderr) console.error(res.stderr.trimEnd().slice(0, 1000));
        failed++;
        try {
          rmSync(u.obj);
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
export async function createArchive(
  tools: BuildTools,
  archivePath: string,
  objFiles: string[],
): Promise<void> {
  if (objFiles.length === 0) return;
  mkdirSync(dirname(archivePath), { recursive: true });
  const batch = 200;
  for (let i = 0; i < objFiles.length; i += batch) {
    const chunk = objFiles.slice(i, i + batch);
    const flag = i === 0 ? "rcs" : "rs";
    const res = await spawnCmd([tools.ar, flag, archivePath, ...chunk]);
    if (!res.ok) throw new Error(`ar failed: ${res.stderr}`);
  }
}

export type EmbedFormat = "pe" | "macho";

/** Embed a binary file as a linkable .o (font data for mupdf noto.c). */
export async function embedBinaryFile(
  tools: BuildTools,
  format: EmbedFormat,
  inputFile: string,
  outputObj: string,
  symbolPrefix: string,
): Promise<void> {
  mkdirSync(dirname(outputObj), { recursive: true });
  const outAbsolute = join(process.cwd(), outputObj);
  const cleanFileName = symbolPrefix.replace(/^_binary_/, "");
  const tmpDir = join(dirname(outputObj), "_fonttmp");
  mkdirSync(tmpDir, { recursive: true });
  const tmpInput = join(tmpDir, cleanFileName);
  const data = await readFile(inputFile);
  await writeFile(tmpInput, data);

  let res: { ok: boolean; stderr: string };
  if (format === "pe") {
    res = await spawnCmd(
      [
        tools.embed,
        "-I",
        "binary",
        "-O",
        "pe-x86-64",
        "-B",
        "i386:x86-64",
        "--rename-section",
        ".data=.rodata,CONTENTS,ALLOC,LOAD,READONLY,DATA",
        "--redefine-sym",
        `_binary_${cleanFileName}_start=${symbolPrefix}`,
        cleanFileName,
        outAbsolute,
      ],
      { cwd: tmpDir },
    );
  } else {
    // Mach-O: Apple's ld lacks GNU objcopy/ld -b binary. Use xxd + clang.
    const cSrc = join(dirname(outputObj), `${cleanFileName}.c`);
    const xxd = Bun.which("xxd");
    if (!xxd) {
      throw new Error("xxd not found (needed to embed fonts on macOS)");
    }
    const xxdRes = await spawnCmd([xxd, "-i", cleanFileName], { cwd: tmpDir, captureStdout: true });
    if (!xxdRes.ok) {
      throw new Error(`xxd failed for ${inputFile}: ${xxdRes.stderr}`);
    }
    // xxd -i names symbols from the filename: <name>[] and <name>_len
    const cBody = xxdRes.stdout
      .replace(
        new RegExp(`unsigned char ${cleanFileName}\\[\\]`),
        `const unsigned char ${symbolPrefix}[]`,
      )
      .replace(
        new RegExp(`unsigned int ${cleanFileName}_len`),
        `const unsigned int ${symbolPrefix}_size`,
      );
    await writeFile(cSrc, cBody);
    res = await spawnCmd([tools.cc, "-Os", "-c", cSrc, "-o", outAbsolute]);
    if (!res.ok) {
      throw new Error(`Failed to compile embedded font ${inputFile}: ${res.stderr}`);
    }
    return;
  }

  if (!res.ok) {
    throw new Error(`Failed to embed ${inputFile}: ${res.stderr}`);
  }
  try {
    await Bun.write(tmpInput, "");
  } catch {}
}

export async function buildLibrary(
  lib: LibDef,
  outDir: string,
  isRelease: boolean,
  opts: BuildLibraryOptions,
): Promise<{ archive: string; objs: string[] }> {
  const { tools, commonDefines, cxxFlags, jobs, extraObjs } = opts;
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

  const objs = [...units.map((u) => u.obj), ...(extraObjs ?? [])];
  const archivePath = join(outDir, "lib", `lib${lib.name}.a`);
  await createArchive(tools, archivePath, objs);
  console.log(`  -> ${archivePath}`);
  return { archive: archivePath, objs };
}