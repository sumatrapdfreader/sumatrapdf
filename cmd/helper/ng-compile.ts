// Compiles, archives and links targets from targets.ts with the toolchain
// from toolchain.ts. No generator in between: every object is one compiler
// spawn, run N at a time, skipped when the object is newer than its source
// and every header the last compile recorded (cl /sourceDependencies JSON,
// -MMD .d files everywhere else, clang-cl included).
//
// Output: out/<platform>/<cfg>/<target>.exe (apps, tools), lib/ (static libs)
// and obj/<target>/<source path>.obj beneath the same directory.

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { cpus } from "node:os";
import { basename, dirname, isAbsolute, join, relative } from "node:path";
import { Glob } from "bun";
import type { Platform, Toolchain } from "./ng-toolchain";
import { findTarget, forPlatform, sourceBuildsOn, type Target, type TargetKind } from "./ng-targets";
import { isShared, sharedFiles, sharedPath, sharedSource } from "./ng-shared";

export type BuildFlags = {
  debug: boolean;
  asan: boolean;
  /** orig's Profile configuration: IS_PERF_LOG=1 and /callcap (Windows, cl.exe) */
  profile?: boolean;
  clang: boolean;
  clean: boolean;
  verbose: boolean;
};

export type Fail = (msg: string) => never;

export const root = join(import.meta.dir, "..", "..");

function sharedDir(dir: string): string {
  return join(dir, "generated", "shared");
}

// Stage shared files so quoted includes resolve ng headers before original headers.
export function stageShared(dir: string): void {
  for (const src of sharedFiles) {
    const dst = join(sharedDir(dir), sharedPath(src));
    let data = readFileSync(join(root, src));
    if (/\.(cpp|c|h)$/.test(src)) {
      const text = data
        .toString("utf8")
        .replace(/(#include ")(?:\.\.\/)+ext\//g, `$1${join(root, "ext").replaceAll("\\", "/")}/`);
      data = Buffer.from(text);
    }
    if (existsSync(dst) && readFileSync(dst).equals(data)) continue;
    mkdirSync(dirname(dst), { recursive: true });
    writeFileSync(dst, data);
  }
}

function withShared(t: Target, dir: string): Target {
  return {
    ...t,
    includes: (t.includes ?? []).flatMap((p) =>
      p.startsWith("src/ng") ? [p, relative(root, join(sharedDir(dir), p.replace(/^src\/ng/, "src")))] : [p],
    ),
  };
}

// ─── output layout ────────────────────────────────────────────────────────

export function outDirName(plat: Platform, f: BuildFlags): string {
  let name = f.debug ? "dbg" : "rel";
  // mac is clang either way, so -clang only names a separate output dir where
  // it selects a different compiler
  if (f.clang && plat !== "mac") name += "-clang";
  if (f.asan) name += "-asan";
  if (f.profile) name += "-profile";
  return `${plat}/${name}`;
}

export function outDir(plat: Platform, f: BuildFlags): string {
  return join(root, "out", outDirName(plat, f));
}

// on wasm an app is a page and a console tool is a node script; em++ picks
// what it emits from the extension
export function exeName(plat: Platform, name: string, kind: TargetKind): string {
  if (plat === "win") return `${name}.exe`;
  if (plat === "wasm") return kind === "app" ? `${name}.html` : `${name}.js`;
  return name;
}

function libName(tc: Toolchain, name: string): string {
  return tc.plat === "win" ? `${name}.lib` : `lib${name}.a`;
}

function objPath(dir: string, target: Target, src: string, tc: Toolchain): string {
  const rel = src.replaceAll("\\", "/");
  return join(dir, "obj", target.name, `${rel}.${tc.objExt}`);
}

// ─── sources ──────────────────────────────────────────────────────────────

export function resolveSources(t: Target, plat: Platform, fail: Fail): string[] {
  return resolveGlobs(t, t.sources, t.exclude ?? [], plat, fail);
}

function resolveGlobs(t: Target, sources: string[], exclude: string[], plat: Platform, fail: Fail): string[] {
  const excluded = exclude.map((p) => new Glob(p));
  const out: string[] = [];
  for (const pattern of sources) {
    const g = new Glob(pattern);
    const matches = Array.from(g.scanSync({ cwd: root, dot: false })).map((p) => p.replaceAll("\\", "/"));
    if (pattern.startsWith("src/")) {
      matches.push(
        ...sharedFiles.filter((p) => {
          const path = sharedPath(p);
          return g.match(pattern.startsWith("src/ng/") ? path.replace(/^src\//, "src/ng/") : path);
        }),
      );
    }
    if (matches.length === 0) fail(`target ${t.name}: no files match ${pattern}`);
    for (const m of matches.sort()) {
      if (!sourceBuildsOn(m, plat)) continue;
      if (
        excluded.some(
          (e) => e.match(m) || e.match(sharedPath(m)) || e.match(sharedPath(m).replace(/^src\//, "src/ng/")),
        )
      )
        continue;
      if (!out.includes(m)) out.push(m);
    }
  }
  return out;
}

// NASM sources, on the platforms the target says have an assembler.
function resolveAsmSources(t: Target, plat: Platform, f: BuildFlags, fail: Fail): string[] {
  const a = t.asm;
  if (!a) return [];
  if (a.profileOnly && !f.profile) return [];
  if (a.platforms && !a.platforms.includes(plat)) return [];
  // bin/nasm.exe is the only assembler we ship
  if (plat !== "win") return [];
  return resolveGlobs(t, a.sources, a.exclude ?? [], plat, fail);
}

// Writes the target's config headers into out/<cfg>/generated/<target>/ and
// returns the target with that directory first on the include path. Rewrites
// only on a change, so an unchanged header does not invalidate the objects.
function withGenerated(t: Target, plat: Platform, dir: string): Target {
  const wanted = (t.generated ?? []).filter((g) => !g.platforms || g.platforms.includes(plat));
  if (wanted.length === 0) return t;
  const genDir = join(dir, "generated", t.name);
  mkdirSync(genDir, { recursive: true });
  for (const g of wanted) {
    const dst = join(genDir, g.name);
    if (g.content === undefined) {
      const src = join(root, g.from!);
      if (mtime(src) > mtime(dst)) copyFileSync(src, dst);
      continue;
    }
    if (!existsSync(dst) || readFileSync(dst, "utf8") !== g.content) writeFileSync(dst, g.content);
  }
  return { ...t, includes: [relative(root, genDir), ...(t.includes ?? [])] };
}

function isCSource(p: string): boolean {
  return /\.c$/.test(p);
}

function isObjC(p: string): boolean {
  return /\.(m|mm)$/.test(p);
}

// ─── flags ────────────────────────────────────────────────────────────────

// Warnings orig disables project-wide (premake5.lua), kept so ported code
// compiles unchanged. 4100 (unused parameter) is the counterpart of gcc's
// -Wno-unused-parameter below; orig disables it in every project that sees
// mupdf's headers, where FZ_UNUSED is a no-op on MSVC.
const msvcDisabledWarnings = [
  "4100",
  "4127",
  "4189",
  "4324",
  "4457",
  "4458",
  "4522",
  "4611",
  "4702",
  "4800",
  "4838",
  "6319",
  "4996",
];

// clang-cl diagnoses things cl.exe does not, all over code ported from orig
// (which only ever had to satisfy cl /W4). Off, so -clang and cl compile the
// same sources: unused locals/constants are cl's 4189 (already off above),
// pragma-pack is the Windows SDK's own pshpack8.h / poppack.h idiom, and the
// rest are style warnings cl has no equivalent for.
const clangOnlyWarnings = [
  "-Wno-invalid-offsetof",
  "-Wno-logical-op-parentheses",
  "-Wno-pragma-pack",
  "-Wno-reorder-ctor",
  "-Wno-sign-compare",
  "-Wno-switch",
  "-Wno-undefined-bool-conversion",
  "-Wno-unneeded-internal-declaration",
  "-Wno-unused-const-variable",
  "-Wno-unused-variable",
];

// gcc 14+ and clang 16+ make these legacy-C diagnostics errors, which -w does
// not undo. Third-party C (mupdf's load-jxr-win.c, zlib's gzlib) still writes
// that way and we do not edit ext/. Same list as orig's dependency build.
const legacyCWarnings = [
  "-Wno-implicit-function-declaration",
  "-Wno-implicit-int",
  "-Wno-incompatible-pointer-types",
  "-Wno-int-conversion",
];

// the same list for gcc/clang/em++ builds, in the names both accept
const gccOnlyWarnings = [
  // a `//` comment whose last character is a backslash (a registry path in a
  // CrashHandler comment); cl.exe says nothing
  "-Wno-comment",
  "-Wno-implicit-fallthrough",
  "-Wno-invalid-offsetof",
  "-Wno-parentheses",
  "-Wno-reorder",
  "-Wno-sign-compare",
  "-Wno-switch",
  // MSVC-only pragmas: `#pragma warning(suppress: N)` in ported code
  "-Wno-unknown-pragmas",
  "-Wno-unused-but-set-variable",
  "-Wno-unused-const-variable",
  "-Wno-unused-function",
  "-Wno-unused-variable",
];

function definesOf(t: Target, f: BuildFlags): string[] {
  const extra = (f.debug ? t.debugDefines : t.releaseDefines) ?? [];
  // orig's Profile configuration defines it for every project
  const profile = f.profile ? ["IS_PERF_LOG=1"] : [];
  return [...(t.defines ?? []), ...extra, ...profile];
}

function msvcCflags(t: Target, f: BuildFlags, cpp: boolean): string[] {
  const flags = ["/nologo", "/utf-8", "/DUNICODE", "/D_UNICODE", "/D_CRT_SECURE_NO_WARNINGS", "/DIS_TRACY=0"];
  if (cpp) {
    flags.push("/std:c++20", "/GR-");
    flags.push(...(t.exceptions ? ["/EHsc"] : ["/EHs-c-", "/D_HAS_EXCEPTIONS=0"]));
  }
  if (t.strict) {
    flags.push("/W4", "/WX", "/we4840");
    for (const w of msvcDisabledWarnings) flags.push(`/wd${w}`);
  } else {
    // third-party: warnings are noise we will not fix
    flags.push("/W0");
  }
  // static CRT (no vcruntime dll); /Gy /Gw so the linker drops unused code.
  // third-party libs are optimized in debug builds too, as in orig
  const dbgOpt = t.alwaysOptimize ? "/O1" : "/Od";
  // orig's optimized_conf(): a lib that is optimized in debug builds is also
  // compiled NDEBUG there (libarchive's DEBUG blocks print to stderr)
  const dbgDefine = t.alwaysOptimize ? "/DNDEBUG" : "/DDEBUG";
  flags.push(...(f.debug ? [dbgOpt, "/MTd", dbgDefine] : ["/O2", "/Gy", "/Gw", "/GF", "/MT", "/DNDEBUG"]));
  if (f.clang) {
    // same baseline as orig's clang build: cl.exe emits SSE4.1/AVX2 intrinsics
    // (mupdf's deskew.c, heicdec) without asking, clang-cl wants the target
    flags.push("/Z7", "-Wno-unused-command-line-argument", "-march=x86-64-v3", "-maes");
    if (t.strict) {
      flags.push("-Wno-missing-field-initializers", "-Wno-microsoft-exception-spec");
      flags.push(...clangOnlyWarnings);
    } else if (!cpp) {
      flags.push(...legacyCWarnings);
    }
  } else {
    flags.push("/FS", "/Zi");
  }
  if (f.asan) {
    flags.push("/fsanitize=address");
    if (!f.clang) flags.push("/bigobj");
  }
  // MSVC /callcap inserts _CAP_Enter_Function / _CAP_Exit_Function at every
  // function entry / exit; the hooks live in the uninstrumented base
  if (f.profile && t.callcap) flags.push("/callcap");
  for (const d of definesOf(t, f)) flags.push(`/D${d}`);
  for (const i of t.includes ?? []) flags.push("/I", join(root, i));
  return flags;
}

function gccCflags(tc: Toolchain, t: Target, f: BuildFlags, cpp: boolean): string[] {
  const flags = ["-DIS_TRACY=0"];
  if (cpp) {
    flags.push("-std=c++20", "-fno-rtti");
    if (!t.exceptions) flags.push("-fno-exceptions");
  } else {
    flags.push("-std=gnu11");
  }
  if (t.strict) {
    flags.push("-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-missing-field-initializers");
    flags.push(...gccOnlyWarnings);
    // same diagnostic, two names: `if (!this)` in Arena
    flags.push(tc.gnu ? "-Wno-nonnull-compare" : "-Wno-undefined-bool-conversion");
    // gcc only: memset over a struct with a member that has a constructor
    // (our Str). cl.exe and clang have no such warning, and orig's code
    // zero-fills its own PODs that way all over
    // type-limits: `ReportIf(unsignedIndex < 0)` in code that is signed on
    // Windows. gcc only, again with no cl.exe or clang counterpart
    // clobbered: every mupdf fz_try() is a setjmp, so gcc suspects every local
    // around it. cl.exe (and clang) have no such warning
    // strict-aliasing: reading a float's bits through a u32* to hash it
    if (tc.gnu) flags.push("-Wno-class-memaccess", "-Wno-type-limits", "-Wno-clobbered", "-Wno-strict-aliasing");
  } else {
    flags.push("-w");
    if (!cpp) flags.push(...legacyCWarnings);
  }
  flags.push(
    ...(f.debug
      ? [t.alwaysOptimize ? "-O1" : "-O0", "-g", t.alwaysOptimize ? "-DNDEBUG" : "-DDEBUG"]
      : ["-O2", "-DNDEBUG"]),
  );
  if (tc.plat === "mac") flags.push("-Wno-deprecated-declarations", "-g");
  if (tc.plat === "linux") flags.push("-g");
  if (tc.plat === "wasm" && f.debug) flags.push("-g");
  if (f.asan) flags.push("-fsanitize=address", "-fno-omit-frame-pointer");
  for (const d of definesOf(t, f)) flags.push(`-D${d}`);
  for (const i of t.includes ?? []) flags.push("-I", join(root, i));
  if (tc.plat === "linux") {
    // after the target's own -I dirs, and as -isystem so they are searched
    // last: pkg-config's -I/usr/include/freetype2 would otherwise shadow
    // ext/a-freetype's <freetype/*.h> from inside its own internal headers
    for (const a of linuxDeps().cflags) {
      if (a.startsWith("-I")) flags.push("-isystem", a.slice(2));
      else flags.push(a);
    }
  }
  return flags;
}

function perSourceFlags(t: Target, src: string, tc: Toolchain): string[] {
  const out: string[] = [];
  for (const ps of t.perSource ?? []) {
    if (ps.platforms && !ps.platforms.includes(tc.plat)) continue;
    const glob = new Glob(ps.glob);
    const path = sharedPath(src);
    if (!glob.match(src) && !glob.match(path) && !glob.match(path.replace(/^src\//, "src/ng/"))) continue;
    out.push(...(tc.msvcStyle ? (ps.msvcFlags ?? []) : ps.flags));
  }
  return out;
}

// ─── Linux pkg-config ─────────────────────────────────────────────────────

const linuxPkgs = ["x11", "cairo", "pangocairo", "gdk-pixbuf-2.0", "gio-2.0", "fontconfig", "openssl"];
let linuxDepsMemo: { cflags: string[]; libs: string[] } | null = null;

function pkgConfig(kind: "--cflags" | "--libs", names: string[]): string[] {
  const r = Bun.spawnSync(["pkg-config", kind, ...names], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    console.error(r.stderr.toString().trim());
    console.error(
      `pkg-config ${kind} ${names.join(" ")} failed. sudo apt install libx11-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev libssl-dev`,
    );
    process.exit(1);
  }
  return r.stdout.toString().trim().split(/\s+/).filter(Boolean);
}

function linuxDeps(): { cflags: string[]; libs: string[] } {
  if (linuxDepsMemo) return linuxDepsMemo;
  const cflags = pkgConfig("--cflags", linuxPkgs);
  cflags.push("-DSUMATRA_HAVE_OPENSSL=1");
  const libs = pkgConfig("--libs", linuxPkgs);
  const curl = Bun.spawnSync(["pkg-config", "--exists", "libcurl"], { stdout: "pipe", stderr: "pipe" });
  if (curl.exitCode === 0) {
    cflags.push(...pkgConfig("--cflags", ["libcurl"]), "-DGPUI_HAVE_CURL=1");
    libs.push(...pkgConfig("--libs", ["libcurl"]));
  }
  linuxDepsMemo = { cflags, libs };
  return linuxDepsMemo;
}

// ─── dependency tracking ──────────────────────────────────────────────────

function mtime(p: string): number {
  try {
    return statSync(p).mtimeMs;
  } catch {
    return -1;
  }
}

// Headers the last compile of `obj` read, from the compiler's own record.
function recordedDeps(obj: string, msvc: boolean): string[] | null {
  const depFile = msvc ? `${obj}.json` : `${obj}.d`;
  if (!existsSync(depFile)) return null;
  const text = readFileSync(depFile, "utf8");
  if (msvc) {
    try {
      const j = JSON.parse(text);
      return (j?.Data?.Includes ?? []) as string[];
    } catch {
      return null;
    }
  }
  // make syntax: "obj: src \\\n hdr hdr ..." with backslash continuations.
  // The target can be an absolute Windows path, so skip its drive colon.
  let colon = -1;
  for (;;) {
    colon = text.indexOf(":", colon + 1);
    if (colon < 0) return null;
    const isDrive = /[/\\]/.test(text[colon + 1] ?? "") && /[A-Za-z]/.test(text[colon - 1] ?? "");
    if (!isDrive) break;
  }
  return text
    .slice(colon + 1)
    .replace(/\\\r?\n/g, " ")
    .split(/\s+/)
    .filter(Boolean)
    .map((p) => p.replace(/\\ /g, " "));
}

// `msvc` selects the dependency record format, not the compiler: NASM writes
// make-syntax .d files like gcc does.
function needsCompile(src: string, obj: string, msvc: boolean): boolean {
  const objTime = mtime(obj);
  if (objTime < 0) return true;
  if (mtime(join(root, src)) > objTime) return true;
  const deps = recordedDeps(obj, msvc);
  if (!deps) return true;
  for (const d of deps) {
    // gcc and nasm echo the source path as we passed it: relative to the root
    const t = mtime(isAbsolute(d) ? d : join(root, d));
    if (t < 0 || t > objTime) return true;
  }
  return false;
}

// ─── spawning ─────────────────────────────────────────────────────────────

export function formatCmd(cmd: string[]): string {
  return cmd.map((a) => (/[\s"]/.test(a) ? `"${a.replaceAll('"', '\\"')}"` : a)).join(" ");
}

type SpawnResult = { code: number; out: string };

// Windows environment names are case-insensitive but a JS object's are not:
// the shell may hand us `Path` while vcvars gave us `PATH`, and a child
// process given both picks one at random. The toolchain's spelling wins.
// An empty override removes the variable (emsdk clears PYTHONHOME that way).
export function mergeEnv(base: NodeJS.ProcessEnv, over: Record<string, string>): Record<string, string> {
  const out: Record<string, string> = {};
  const overUpper = new Set(Object.keys(over).map((k) => k.toUpperCase()));
  for (const [k, v] of Object.entries(base)) {
    if (v !== undefined && !overUpper.has(k.toUpperCase())) out[k] = v;
  }
  for (const [k, v] of Object.entries(over)) {
    if (v !== "") out[k] = v;
  }
  return out;
}

async function spawn(tc: Toolchain, cmd: string[]): Promise<SpawnResult> {
  const p = Bun.spawn(cmd, {
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
    env: mergeEnv(process.env, tc.env),
  });
  const [out, err, code] = await Promise.all([new Response(p.stdout).text(), new Response(p.stderr).text(), p.exited]);
  return { code, out: out + err };
}

// cl.exe echoes the source file name on the first line; drop it so a clean
// compile prints nothing.
function stripClEcho(out: string, src: string): string {
  const lines = out.split(/\r?\n/);
  if (lines.length > 0 && lines[0]!.trim() === basename(src)) lines.shift();
  return lines.join("\n").trim();
}

// Writes args to a response file so lib/link/ar never hit the command line
// length limit.
function responseFile(dir: string, name: string, args: string[]): string {
  const p = join(dir, `${name}.rsp`);
  writeFileSync(p, args.map((a) => (/[\s]/.test(a) ? `"${a}"` : a)).join("\n") + "\n");
  return p;
}

// ─── compile one target ───────────────────────────────────────────────────

type CompileJob = { src: string; obj: string; cmd: string[]; msvcDeps: boolean };

// clang-cl accepts /sourceDependencies but ignores it (warns "argument
// unused"), so it gets gcc-style .d files through the /clang: escape instead.
function usesMsvcDeps(tc: Toolchain, f: BuildFlags): boolean {
  return tc.msvcStyle && !f.clang;
}

const nasm = join(root, "bin", "nasm.exe");

function asmCmd(t: Target, src: string, obj: string): string[] {
  const a = t.asm!;
  const cmd = [nasm, "-f", "win64"];
  for (const d of a.defines ?? []) cmd.push(`-D${d}`);
  for (const i of a.includes ?? []) cmd.push("-I", `${join(root, i)}\\`);
  cmd.push("-MD", `${obj}.d`, "-o", obj, src);
  return cmd;
}

function compileCmd(tc: Toolchain, t: Target, f: BuildFlags, src: string, obj: string): string[] {
  const cpp = !isCSource(src);
  const extra = perSourceFlags(t, src, tc);
  if (tc.msvcStyle) {
    const flags = msvcCflags(t, f, cpp);
    const lang = cpp ? "/TP" : "/TC";
    const deps = f.clang ? ["/clang:-MMD", "/clang:-MF", `/clang:${obj}.d`] : ["/sourceDependencies", `${obj}.json`];
    return [tc.cxx, ...flags, ...extra, lang, "/c", src, `/Fo${obj}`, `/Fd${dirname(obj)}\\`, ...deps];
  }
  const flags = gccCflags(tc, t, f, cpp);
  const cmd = [cpp ? tc.cxx : tc.cc, ...flags, ...extra];
  if (tc.plat === "mac" && (isObjC(src) || /ext\/gpui\/gpui\.cpp$/.test(src))) {
    cmd.push("-x", "objective-c++", "-fobjc-arc");
  }
  cmd.push("-MMD", "-MF", `${obj}.d`, "-c", src, "-o", obj);
  return cmd;
}

async function runPool<T>(jobs: T[], n: number, fn: (job: T) => Promise<boolean>): Promise<boolean> {
  let next = 0;
  let ok = true;
  const worker = async () => {
    while (ok && next < jobs.length) {
      const job = jobs[next++]!;
      if (!(await fn(job))) ok = false;
    }
  };
  await Promise.all(Array.from({ length: Math.min(n, jobs.length) }, worker));
  return ok;
}

async function compileTarget(tc: Toolchain, t0: Target, f: BuildFlags, dir: string, fail: Fail): Promise<string[]> {
  const t = withGenerated(withShared(forPlatform(t0, tc.plat), dir), tc.plat, dir);
  const sources = resolveSources(t, tc.plat, fail);
  const asmSources = resolveAsmSources(t, tc.plat, f, fail);
  const objs: string[] = [];
  const jobs: CompileJob[] = [];

  // a flags change invalidates every object of the target
  const stamp = join(dir, "obj", t.name, "flags.txt");
  const key = [tc.cxx, ...msvcOrGccKey(tc, t, f), ...(asmSources.length ? asmCmd(t, "", "") : [])].join(" ");
  const flagsChanged = !existsSync(stamp) || readFileSync(stamp, "utf8") !== key;

  const msvcDeps = usesMsvcDeps(tc, f);
  for (const src of sources) {
    const obj = objPath(dir, t, src, tc);
    objs.push(obj);
    if (!flagsChanged && !needsCompile(src, obj, msvcDeps)) continue;
    mkdirSync(dirname(obj), { recursive: true });
    const cmd = compileCmd(tc, t, f, src, obj);
    if (isShared(src)) {
      const staged = join(sharedDir(dir), sharedPath(src));
      for (let i = 0; i < cmd.length; i++) if (cmd[i] === src) cmd[i] = staged;
    }
    jobs.push({ src, obj, cmd, msvcDeps });
  }
  for (const src of asmSources) {
    const obj = objPath(dir, t, src, tc);
    objs.push(obj);
    if (!flagsChanged && !needsCompile(src, obj, false)) continue;
    mkdirSync(dirname(obj), { recursive: true });
    jobs.push({
      src,
      obj,
      cmd: asmCmd(t, isShared(src) ? join(sharedDir(dir), sharedPath(src)) : src, obj),
      msvcDeps: false,
    });
  }
  if (jobs.length === 0) {
    console.log(`  ${t.name}: up to date`);
    return objs;
  }
  console.log(`  ${t.name}: compiling ${jobs.length} of ${sources.length + asmSources.length} files`);
  const started = performance.now();
  const ok = await runPool(jobs, cpus().length, async (job) => {
    if (f.verbose) console.log(`> ${formatCmd(job.cmd)}`);
    const r = await spawn(tc, job.cmd);
    const text = job.msvcDeps ? stripClEcho(r.out, job.src) : r.out.trim();
    if (r.code !== 0) {
      rmSync(job.obj, { force: true });
      console.error(`\nerror compiling ${job.src}:\n${text}\n> ${formatCmd(job.cmd)}`);
      return false;
    }
    if (text) console.log(text);
    return true;
  });
  if (!ok) fail(`build of ${t.name} failed`);
  mkdirSync(dirname(stamp), { recursive: true });
  writeFileSync(stamp, key);
  console.log(`  ${t.name}: compiled in ${((performance.now() - started) / 1000).toFixed(1)} s`);
  return objs;
}

// the per-source overrides go into the key too: they are not in the common
// flags, so a change to one would otherwise leave every object up to date
// ─── Windows resources ────────────────────────────────────────────────────

// rc.exe writes no dependency file, so scan the script for what it names:
// #include "x.h" and the quoted path of every resource statement.
function rcDeps(rcPath: string): string[] {
  const out: string[] = [];
  const visit = (p: string) => {
    if (!existsSync(p)) p = join(root, sharedSource(relative(root, p).replaceAll("\\", "/")));
    if (out.includes(p) || !existsSync(p)) return;
    out.push(p);
    if (!/\.(rc|h)$/i.test(p)) return;
    const dir = dirname(p);
    const text = readFileSync(p, "utf8");
    for (const m of text.matchAll(/^\s*#include\s+"([^"]+)"/gm)) visit(join(dir, m[1]!));
    for (const m of text.matchAll(/^\s*\S+\s+(?:ICON|BITMAP|CURSOR|RCDATA|RT_MANIFEST)\s+"([^"]+)"/gm)) {
      visit(join(dir, m[1]!.replaceAll("\\\\", "\\")));
    }
  };
  visit(rcPath);
  return out;
}

// Compiles the target's .rc into out/<cfg>/<target>.res, which link.exe takes
// as another input. orig's premake does the same with resdefines / .rc files.
async function compileRc(tc: Toolchain, t: Target, f: BuildFlags, dir: string, fail: Fail): Promise<string> {
  const src = join(root, t.rc!);
  const out = join(dir, `${t.name}.res`);
  const deps = rcDeps(src);
  if (existsSync(out) && deps.every((d) => mtime(d) <= mtime(out))) return out;
  if (!tc.rc) fail(`target ${t.name}: rc.exe not found (no Windows SDK in the toolchain)`);
  const cmd = [
    tc.rc,
    "/nologo",
    ...(f.debug ? ["/d_DEBUG"] : []),
    "/i",
    dirname(src),
    "/i",
    join(sharedDir(dir), "src"),
    "/fo",
    out,
    src,
  ];
  console.log(`  ${t.name}: compiling ${relative(root, src)}`);
  if (f.verbose) console.log(`> ${formatCmd(cmd)}`);
  const r = await spawn(tc, cmd);
  if (r.code !== 0) {
    console.error(r.out);
    fail(`compiling ${relative(root, src)} failed`);
  }
  return out;
}

function msvcOrGccKey(tc: Toolchain, t: Target, f: BuildFlags): string[] {
  const common = tc.msvcStyle ? msvcCflags(t, f, true) : gccCflags(tc, t, f, true);
  const perSrc = (t.perSource ?? [])
    .filter((ps) => !ps.platforms || ps.platforms.includes(tc.plat))
    .map((ps) => `${ps.glob}:${(tc.msvcStyle ? (ps.msvcFlags ?? []) : ps.flags).join(" ")}`);
  return [...common, ...perSrc];
}

// ─── archive and link ─────────────────────────────────────────────────────

async function archive(tc: Toolchain, t: Target, dir: string, objs: string[], fail: Fail): Promise<string> {
  const libDir = join(dir, "lib");
  mkdirSync(libDir, { recursive: true });
  const out = join(libDir, libName(tc, t.name));
  // the member list, so dropping a source rearchives instead of leaving its
  // object in the lib (mtimes alone never notice a removal)
  const listPath = join(libDir, `${t.name}.objs`);
  const list = objs.join("\n");
  const listSame = existsSync(listPath) && readFileSync(listPath, "utf8") === list;
  if (listSame && existsSync(out) && objs.every((o) => mtime(o) <= mtime(out))) return out;
  writeFileSync(listPath, list);
  let cmd: string[];
  rmSync(out, { force: true });
  if (tc.plat === "win") {
    const rsp = responseFile(libDir, t.name, [`/OUT:${out}`, ...objs]);
    cmd = [tc.ar, "/nologo", `@${rsp}`];
  } else if (tc.plat === "mac") {
    // Apple's ar has no @response-file support; its libtool takes a list
    // file (one path per line, unquoted) instead
    const list = join(libDir, `${t.name}.rsp`);
    writeFileSync(list, objs.join("\n") + "\n");
    cmd = [tc.ar, "-static", "-no_warning_for_no_symbols", "-o", out, "-filelist", list];
  } else {
    const rsp = responseFile(libDir, t.name, objs);
    cmd = [tc.ar, "rcs", out, `@${rsp}`];
  }
  const r = await spawn(tc, cmd);
  if (r.code !== 0) {
    console.error(r.out);
    fail(`archiving ${t.name} failed: ${formatCmd(cmd)}`);
  }
  return out;
}

// ext/gpui/web/shell.html is vendored, so the page we serve is written into
// the output dir: gpui's shell with src/gui/WasmShell.js spliced in ahead of
// the module script, which is what mounts IDBFS before main() runs.
function wasmShellFile(dir: string): string {
  const shell = readFileSync(join(root, "ext", "gpui", "web", "shell.html"), "utf8");
  const js = readFileSync(join(root, "src", "ng", "gui", "WasmShell.js"), "utf8");
  const marker = "{{{ SCRIPT }}}";
  // the marker also appears in the shell's opening comment, which explains it;
  // the real one is the last
  const at = shell.lastIndexOf(marker);
  if (at < 0) throw new Error("ext/gpui/web/shell.html has no {{{ SCRIPT }}} marker");
  const spliced = `${shell.slice(0, at)}<script>\n${js}\n</script>\n    ${shell.slice(at)}`;
  const out = spliced.replace("<title>gpui</title>", "<title>SumatraPDF</title>");
  const path = join(dir, "generated", "shell.html");
  mkdirSync(dirname(path), { recursive: true });
  if (!existsSync(path) || readFileSync(path, "utf8") !== out) writeFileSync(path, out);
  return path;
}

// --preload-file pairs for the files the app reads from MEMFS at runtime
function wasmPreloadArgs(t: Target, fail: Fail): string[] {
  const args: string[] = [];
  for (const p of t.wasmPreload ?? []) {
    const matches = Array.from(new Glob(p.from).scanSync({ cwd: root, dot: false })).sort();
    if (matches.length === 0) fail(`target ${t.name}: no files match ${p.from}`);
    for (const m of matches) {
      const name = basename(m);
      args.push("--preload-file", `${join(root, m)}@${p.to}/${name}`);
    }
  }
  return args;
}

const winNoDefaultLibs = ["msvcrt.lib", "msvcrtd.lib", "ucrt.lib", "ucrtd.lib", "vcruntime.lib", "vcruntimed.lib"];

// Frameworks required by the mac app and GPUI's speech and webview backends.
const macFrameworks = [
  "AudioToolbox",
  "AVFoundation",
  "Cocoa",
  "CoreText",
  "CoreGraphics",
  "ImageIO",
  "IOKit",
  "Security",
  "Speech",
  "WebKit",
];

// mac's iconv is GNU libiconv in its own dylib, not in libSystem as on glibc;
// a-libarchive's charset conversion needs it
const macLibs = ["-lcurl", "-liconv"];

async function link(
  tc: Toolchain,
  t: Target,
  f: BuildFlags,
  dir: string,
  objs: string[],
  libs: string[],
  fail: Fail,
): Promise<string> {
  const out = join(dir, exeName(tc.plat, t.name, t.kind));
  const inputs = [...objs, ...libs];
  const { cmd, key } = linkCmd(tc, t, f, dir, out, inputs, fail);
  // the command line itself is an input: a changed flag relinks
  const cmdStamp = join(dir, `${t.name}-link.txt`);
  const cmdChanged = !existsSync(cmdStamp) || readFileSync(cmdStamp, "utf8") !== key;
  if (!cmdChanged && existsSync(out) && inputs.every((o) => mtime(o) <= mtime(out))) {
    console.log(`  ${t.name}: link up to date`);
    return out;
  }
  console.log(`  ${t.name}: linking ${relative(root, out)}`);
  if (f.verbose) console.log(`> ${formatCmd(cmd)}`);
  const r = await spawn(tc, cmd);
  if (r.code !== 0) {
    console.error(r.out);
    fail(`linking ${t.name} failed: ${formatCmd(cmd)}`);
  }
  const text = r.out.trim();
  if (text) console.log(text);
  writeFileSync(cmdStamp, key);
  return out;
}

// The linker command line and a key that changes when its arguments do; on
// Windows the arguments go through a response file the command names.
function linkCmd(
  tc: Toolchain,
  t: Target,
  f: BuildFlags,
  dir: string,
  out: string,
  inputs: string[],
  fail: Fail,
): { cmd: string[]; key: string } {
  if (tc.plat === "win") {
    const usesGpui = (t.deps ?? []).includes("gpui");
    const args = [
      `/OUT:${out}`,
      t.kind === "app" ? "/SUBSYSTEM:WINDOWS" : "/SUBSYSTEM:CONSOLE",
      // gpui.cpp owns wWinMain and calls GpuiMain
      ...(t.kind === "app" && usesGpui ? ["/ENTRY:wWinMainCRTStartup"] : []),
      // the .rc carries RT_MANIFEST; no linker-generated one next to it
      ...(t.rc ? ["/MANIFEST:NO"] : []),
      ...winNoDefaultLibs.map((l) => `/NODEFAULTLIB:${l}`),
      ...inputs,
      ...(t.winLibs ?? []),
      "/DEBUG",
      `/PDB:${join(dir, `${t.name}.pdb`)}`,
      ...(f.debug ? [] : ["/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF"]),
      ...(f.asan ? ["/INCREMENTAL:NO"] : []),
    ];
    const rsp = responseFile(dir, `${t.name}-link`, args);
    return { cmd: [tc.linker, "/nologo", `@${rsp}`], key: args.join("\n") };
  }
  if (tc.plat === "wasm") {
    const ld = [
      "-sALLOW_MEMORY_GROWTH=1",
      "-sSTACK_SIZE=8MB",
      "-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,HEAPF32",
      ...(f.debug ? ["-O0", "-g", "-sASSERTIONS=2"] : ["-O2", "-sASSERTIONS=0"]),
    ];
    // the page and the preloaded files are inputs the command line only names
    // by path, so their content goes into the key
    let extraKey = "";
    if (t.kind === "app") {
      // IDBFS holds the settings directory; the page mounts it before main(),
      // which is what FS/IDBFS/addRunDependency on Module are for (a page
      // script is outside the module's scope, where they are plain locals).
      // MINIFY_HTML=0: the -O2 minifier drops the script the shell carries
      ld.push("-sENVIRONMENT=web,worker", "-lidbfs.js", "-sFORCE_FILESYSTEM=1", "-sMINIFY_HTML=0");
      ld.push("-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,HEAPF32,FS,IDBFS,addRunDependency,removeRunDependency");
      const shell = wasmShellFile(dir);
      ld.push("--shell-file", shell);
      const preload = wasmPreloadArgs(t, fail);
      ld.push(...preload);
      extraKey = [readFileSync(shell, "utf8"), ...preload.map((a) => `${a}:${mtime(a.split("@")[0]!)}`)].join("\n");
    } else {
      // a console tool runs under node on the real file system
      ld.push("-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1");
    }
    if (f.asan) ld.push("-fsanitize=address");
    const cmd = [tc.cxx, ...inputs, "-o", out, ...ld];
    return { cmd, key: `${cmd.join("\n")}\n${extraKey}` };
  }
  {
    const ld: string[] = [];
    if (tc.plat === "mac") {
      for (const fw of macFrameworks) ld.push("-framework", fw);
      ld.push(...macLibs);
    } else {
      ld.push(...linuxDeps().libs, "-lm", "-lpthread");
    }
    if (f.asan) ld.push("-fsanitize=address");
    // GNU ld scans each archive once, in order, so a symbol an earlier lib
    // takes from a later one (mupdf's hb_*_impl allocators used by harfbuzz)
    // goes unresolved unless the archives form a group
    const libInputs = tc.plat === "linux" ? ["-Wl,--start-group", ...inputs, "-Wl,--end-group"] : inputs;
    const cmd = [tc.cxx, ...libInputs, "-o", out, ...ld];
    return { cmd, key: cmd.join("\n") };
  }
}

// ─── build a target and its dependencies ──────────────────────────────────

/** Static-library dependencies of `t` in link order (dependencies last). */
export function depsOf(t: Target, fail: Fail): Target[] {
  const out: Target[] = [];
  const visit = (name: string) => {
    const d = findTarget(name);
    if (!d) fail(`target ${t.name}: unknown dependency ${name}`);
    if (d.kind !== "staticlib") fail(`target ${t.name}: dependency ${name} is not a static library`);
    for (const n of d.deps ?? []) visit(n);
    if (!out.includes(d)) out.push(d);
  };
  for (const n of t.deps ?? []) visit(n);
  return out.reverse();
}

export async function buildTarget(tc: Toolchain, t: Target, f: BuildFlags, fail: Fail): Promise<string> {
  const dir = outDir(tc.plat, f);
  mkdirSync(dir, { recursive: true });
  if (t.platforms && !t.platforms.includes(tc.plat)) fail(`target ${t.name} does not build on ${tc.plat}`);

  const libs: string[] = [];
  if (t.kind !== "staticlib") {
    for (const d of depsOf(t, fail)) {
      const objs = await compileTarget(tc, d, f, dir, fail);
      libs.push(await archive(tc, d, dir, objs, fail));
    }
  }
  const objs = await compileTarget(tc, t, f, dir, fail);
  if (t.kind === "staticlib") return archive(tc, t, dir, objs, fail);
  if (t.rc && tc.plat === "win") objs.push(await compileRc(tc, t, f, dir, fail));
  return link(tc, t, f, dir, objs, libs, fail);
}

export function cleanOutDir(plat: Platform, f: BuildFlags): void {
  const dir = outDir(plat, f);
  if (!existsSync(dir)) return;
  console.log(`cleaning ${relative(root, dir)}`);
  rmSync(dir, { recursive: true, force: true });
}
