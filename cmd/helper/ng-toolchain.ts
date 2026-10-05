// Finds the compiler for a platform and the environment it needs.
//
// Windows: cl.exe (or clang-cl with -clang). When not started from a developer
// prompt, Visual Studio is located with vswhere and its vcvars64.bat is run
// once; the INCLUDE/LIB/PATH it exports are cached in out/msvc-env.json so the
// next build does not pay for the batch file again.
// Linux/macOS: g++ / clang++. wasm: em++ from emsdk. On Windows the sdk is
// cloned into .work/emsdk (gitignored) on the first -wasm build.

import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { homedir, tmpdir } from "node:os";
import { dirname, join } from "node:path";

export type Platform = "win" | "linux" | "mac" | "wasm";

export type Toolchain = {
  plat: Platform;
  /** C++ compiler executable. */
  cxx: string;
  /** C compiler executable (same as cxx for cl/clang-cl, gcc/clang otherwise). */
  cc: string;
  /** static library archiver: lib.exe, ar, emar */
  ar: string;
  /** Windows: link.exe next to cl.exe; the compiler driver links elsewhere */
  linker: string;
  /** Windows: rc.exe from the SDK, for a target with an .rc */
  rc?: string;
  /** extra environment for every compiler/linker spawn */
  env: Record<string, string>;
  /** cl.exe-style command line (/Fo, /I) rather than gcc-style (-o, -I) */
  msvcStyle: boolean;
  /** g++/gcc, whose warning flag names differ from clang's in a few places */
  gnu: boolean;
  label: string;
  objExt: string;
};

export function hostPlatform(): Platform {
  if (process.platform === "win32") return "win";
  if (process.platform === "darwin") return "mac";
  return "linux";
}

function isFile(p: string): boolean {
  try {
    return statSync(p).isFile();
  } catch {
    return false;
  }
}

function which(name: string): string | null {
  return Bun.which(name);
}

// ─── Windows ──────────────────────────────────────────────────────────────

const msvcInstallHelp = [
  "Install the C++ toolset, then try again:",
  '  winget install Microsoft.VisualStudio.2026.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"',
  'Or open the Visual Studio Installer and add the "Desktop development with C++" workload.',
].join("\n");

function vswhereExe(): string | null {
  const pf86 = process.env["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)";
  const p = join(pf86, "Microsoft Visual Studio", "Installer", "vswhere.exe");
  return isFile(p) ? p : which("vswhere.exe");
}

// Every Visual Studio with the C++ toolset, newest first (18 = 2026, 17 = 2022).
function vsInstallDirs(): string[] {
  const found: string[] = [];
  const vswhere = vswhereExe();
  if (vswhere) {
    const r = Bun.spawnSync(
      [
        vswhere,
        "-latest",
        "-prerelease",
        "-products",
        "*",
        "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property",
        "installationPath",
      ],
      { stdout: "pipe", stderr: "pipe" },
    );
    if (r.exitCode === 0) {
      for (const line of r.stdout.toString().split(/\r?\n/)) {
        const dir = line.trim();
        if (dir && existsSync(dir)) found.push(dir);
      }
    }
  }
  const versions = ["18", "2026", "17", "2022"];
  const editions = ["Insiders", "Preview", "Enterprise", "Professional", "Community", "BuildTools"];
  for (const base of [process.env["ProgramFiles"] ?? "C:\\Program Files", process.env["ProgramFiles(x86)"] ?? ""]) {
    if (!base) continue;
    for (const v of versions) {
      for (const ed of editions) {
        const dir = join(base, "Microsoft Visual Studio", v, ed);
        if (existsSync(join(dir, "VC", "Auxiliary", "Build", "vcvars64.bat")) && !found.includes(dir)) {
          found.push(dir);
        }
      }
    }
  }
  return found;
}

const keptVars = ["PATH", "INCLUDE", "LIB", "LIBPATH", "VCToolsInstallDir", "WindowsSdkDir", "UCRTVersion"];

// Runs vcvars64.bat through a shim batch file and reads back the variables
// it exports. Starts from an environment without a previous vcvars in it:
// vcvars refuses to run twice in one shell.
function runVcvars(vsDir: string): Record<string, string> | null {
  const bat = join(vsDir, "VC", "Auxiliary", "Build", "vcvars64.bat");
  if (!isFile(bat)) return null;
  const env: Record<string, string> = {};
  for (const [k, v] of Object.entries(process.env)) {
    if (v === undefined) continue;
    const up = k.toUpperCase();
    if (up.startsWith("VSCMD_") || up.startsWith("__VSCMD_")) continue;
    if (["VSINSTALLDIR", "VCINSTALLDIR", "VCTOOLSINSTALLDIR", "DEVENVDIR", "INCLUDE", "LIB", "LIBPATH"].includes(up)) {
      continue;
    }
    env[k] = v;
  }
  const preinit = process.env["__VSCMD_PREINIT_PATH"];
  if (preinit) {
    for (const k of Object.keys(env)) {
      if (k.toUpperCase() === "PATH") delete env[k];
    }
    env["PATH"] = preinit;
  }
  const shim = join(tmpdir(), `sumatra-vcvars-${process.pid}.bat`);
  writeFileSync(shim, ["@echo off", `call "${bat}" >nul 2>&1`, "set", ""].join("\r\n"));
  let r;
  try {
    r = Bun.spawnSync(["cmd.exe", "/c", shim], { stdout: "pipe", stderr: "pipe", env });
  } finally {
    rmSync(shim, { force: true });
  }
  if (r.exitCode !== 0) return null;
  // keyed by upper-cased name: `set` prints the variable as the shell spells
  // it, which is `Path` under PowerShell (GitHub's runners) and `PATH` in a
  // bash-started session, and we look them up by fixed names
  const exported: Record<string, string> = {};
  for (const line of r.stdout.toString().split(/\r?\n/)) {
    const eq = line.indexOf("=");
    if (eq <= 0) continue;
    exported[line.slice(0, eq).toUpperCase()] = line.slice(eq + 1);
  }
  if (!exported["INCLUDE"]) return null;
  const keep: Record<string, string> = {};
  for (const k of keptVars) {
    const v = exported[k.toUpperCase()];
    if (v) keep[k] = v;
  }
  return keep;
}

function vcvarsEnv(root: string, vsDir: string): Record<string, string> | null {
  const cachePath = join(root, "out", "msvc-env.json");
  const bat = join(vsDir, "VC", "Auxiliary", "Build", "vcvars64.bat");
  if (existsSync(cachePath)) {
    try {
      const cached = JSON.parse(readFileSync(cachePath, "utf8"));
      if (cached.vsDir === vsDir && cached.batMtime === statSync(bat).mtimeMs && cached.env?.INCLUDE) {
        return cached.env;
      }
    } catch {
      // fall through: recompute
    }
  }
  const env = runVcvars(vsDir);
  if (!env) return null;
  mkdirSync(join(root, "out"), { recursive: true });
  writeFileSync(cachePath, JSON.stringify({ vsDir, batMtime: statSync(bat).mtimeMs, env }, null, 2));
  return env;
}

function findOnPath(name: string, pathValue: string): string | null {
  for (const dir of pathValue.split(";")) {
    if (!dir) continue;
    const p = join(dir, name);
    if (isFile(p)) return p;
  }
  return null;
}

function clangClInVs(vsDir: string): string | null {
  for (const rel of [
    ["VC", "Tools", "Llvm", "x64", "bin", "clang-cl.exe"],
    ["VC", "Tools", "Llvm", "bin", "clang-cl.exe"],
  ]) {
    const p = join(vsDir, ...rel);
    if (isFile(p)) return p;
  }
  return null;
}

// lib.exe and link.exe live next to cl.exe. Never take link.exe from PATH
// alone: Git for Windows ships a /usr/bin/link.exe that shadows it.
function msvcLinkTools(pathValue: string, clOrClangCl: string): { ar: string; linker: string } {
  const clDir = dirname(findOnPath("cl.exe", pathValue) ?? clOrClangCl);
  const ar = isFile(join(clDir, "lib.exe")) ? join(clDir, "lib.exe") : "lib.exe";
  const linker = isFile(join(clDir, "link.exe")) ? join(clDir, "link.exe") : "link.exe";
  return { ar, linker };
}

function findWindowsToolchain(root: string, clang: boolean, fail: (msg: string) => never): Toolchain {
  const wanted = clang ? "clang-cl.exe" : "cl.exe";
  const label = clang ? "clang-cl" : "cl";

  // already in a developer prompt: use it as is
  const onPath = which(wanted);
  if (onPath && process.env["INCLUDE"]) {
    const { ar, linker } = msvcLinkTools(process.env["PATH"] ?? "", onPath);
    return {
      plat: "win",
      cxx: onPath,
      cc: onPath,
      ar,
      linker,
      rc: findOnPath("rc.exe", process.env["PATH"] ?? "") ?? undefined,
      env: {},
      msvcStyle: true,
      gnu: false,
      label,
      objExt: "obj",
    };
  }

  const installs = vsInstallDirs();
  if (installs.length === 0) {
    fail(`${wanted} is not on PATH and no Visual Studio installation was found.\n\n${msvcInstallHelp}`);
  }
  let vcvarsRan = false;
  for (const vs of installs) {
    const env = vcvarsEnv(root, vs);
    if (!env) continue;
    vcvarsRan = true;
    const onVsPath = findOnPath(wanted, env["PATH"] ?? "");
    const exe = clang ? (clangClInVs(vs) ?? onVsPath) : onVsPath;
    if (!exe) continue;
    const { ar, linker } = msvcLinkTools(env["PATH"] ?? "", onVsPath ?? exe);
    const rc = findOnPath("rc.exe", env["PATH"] ?? "") ?? undefined;
    return { plat: "win", cxx: exe, cc: exe, ar, linker, rc, env, msvcStyle: true, gnu: false, label, objExt: "obj" };
  }
  if (clang) {
    fail(
      `Found Visual Studio at ${installs[0]} but no clang-cl.exe in it.\n` +
        'Add "C++ Clang Compiler for Windows" in the Visual Studio Installer, or drop -clang.',
    );
  }
  if (vcvarsRan) {
    fail(
      `Found Visual Studio at ${installs[0]}; its vcvars64.bat ran but put no cl.exe on PATH.\n\n${msvcInstallHelp}`,
    );
  }
  fail(`Found Visual Studio at ${installs[0]} but could not run its vcvars64.bat.\n\n${msvcInstallHelp}`);
}

// ─── Linux / macOS ────────────────────────────────────────────────────────

function findUnixToolchain(plat: "linux" | "mac", clang: boolean, fail: (msg: string) => never): Toolchain {
  const fromEnv = process.env["CXX"];
  // Apple's ar can't read @response files, so mac archives with its libtool
  // (-static -filelist); /usr/bin/libtool is Apple's, GNU's installs as glibtool
  const ar = plat === "mac" ? (which("libtool") ?? "/usr/bin/libtool") : (which("ar") ?? which("llvm-ar") ?? "ar");
  if (fromEnv) {
    const cc = process.env["CC"] ?? fromEnv;
    const gnu = /g\+\+|gcc/.test(fromEnv);
    return { plat, cxx: fromEnv, cc, ar, linker: fromEnv, env: {}, msvcStyle: false, gnu, label: fromEnv, objExt: "o" };
  }
  const order = plat === "mac" ? ["clang++"] : clang ? ["clang++", "g++"] : ["g++", "clang++"];
  for (const name of order) {
    if (!which(name)) continue;
    const cc = name === "g++" ? "gcc" : "clang";
    return {
      plat,
      cxx: name,
      cc: which(cc) ? cc : name,
      ar,
      linker: name,
      env: {},
      msvcStyle: false,
      gnu: name === "g++",
      label: name,
      objExt: "o",
    };
  }
  if (plat === "mac") fail("clang++ not found. Install the command line tools: xcode-select --install");
  fail(`No C++ compiler found (looked for ${order.join(", ")}). sudo apt install build-essential`);
}

// ─── wasm ─────────────────────────────────────────────────────────────────

export function emsdkRoots(repoRoot?: string): string[] {
  const roots: string[] = [];
  if (repoRoot) roots.push(join(repoRoot, ".work", "emsdk"));
  if (process.env["EMSDK"]) roots.push(process.env["EMSDK"]!);
  roots.push(join(homedir(), "emsdk"), "C:\\emsdk", "/opt/emsdk", "/usr/local/emsdk");
  return roots;
}

type Fail = (msg: string) => never;

// emsdk ships em++.exe (a launcher) on Windows and em++.bat next to it in
// older SDKs; off Windows the driver has no extension.
function wasmTools(dir: string): { cxx: string; cc: string; ar: string } | null {
  const bin = join(dir, "upstream", "emscripten");
  const exts = process.platform === "win32" ? [".exe", ".bat", ""] : [""];
  for (const e of exts) {
    const cxx = join(bin, `em++${e}`);
    if (!isFile(cxx)) continue;
    const cc = join(bin, `emcc${e}`);
    const ar = join(bin, `emar${e}`);
    if (isFile(cc) && isFile(ar)) return { cxx, cc, ar };
  }
  return null;
}

function findNamed(dir: string, name: string, depth: number): string | null {
  if (depth < 0 || !existsSync(dir)) return null;
  const direct = join(dir, name);
  if (isFile(direct)) return direct;
  for (const ent of readdirSync(dir, { withFileTypes: true })) {
    if (!ent.isDirectory()) continue;
    const hit = findNamed(join(dir, ent.name), name, depth - 1);
    if (hit) return hit;
  }
  return null;
}

function runChecked(
  cmd: string[],
  cwd: string,
  env: Record<string, string>,
  fail: Fail,
  what: string,
  quiet = false,
): void {
  let r;
  try {
    r = Bun.spawnSync(cmd, { cwd, env, stdout: quiet ? "pipe" : "inherit", stderr: "inherit" });
  } catch (e) {
    fail(`${what} failed to start: ${e}`);
  }
  if (r.exitCode !== 0) {
    const out = quiet ? r.stdout.toString() : "";
    fail(`${what} failed (exit ${r.exitCode})${out ? `\n${out}` : ""}`);
  }
}

// emsdk's bundled CPython breaks when a parent shell exported these.
function envForEmsdk(): Record<string, string> {
  const env: Record<string, string> = {};
  for (const [k, v] of Object.entries(process.env)) {
    if (v === undefined) continue;
    const up = k.toUpperCase();
    if (up === "PYTHONHOME" || up === "PYTHONPATH") continue;
    // Git Bash sets MSYSTEM, which makes emsdk.py write a sh script instead
    // of emsdk_set_env.bat
    if (up === "MSYSTEM") continue;
    env[k] = v;
  }
  env["EMSDK_NOTTY"] = "1";
  return env;
}

function python310(cmd: string[]): string | null {
  let r;
  try {
    r = Bun.spawnSync([...cmd, "-c", "import sys; assert sys.version_info >= (3, 10); print(sys.executable)"], {
      stdout: "pipe",
      stderr: "pipe",
    });
  } catch {
    return null;
  }
  if (r.exitCode !== 0) return null;
  const exe = r.stdout.toString().trim();
  return exe && isFile(exe) ? exe : null;
}

// emsdk.py needs Python 3.10 before it has downloaded its own. The zip is the
// one emsdk would install (see emsdk_manifest.json, id "python" 3.13.3).
function bootstrapPython(root: string, fail: Fail): string {
  const dest = join(root, ".work", "python");
  const already = findNamed(dest, "python.exe", 3);
  if (already) return already;
  const zipName = process.arch === "arm64" ? "python-3.13.3-0-win-arm64.zip" : "python-3.13.3-0-win-amd64.zip";
  const url = `https://storage.googleapis.com/webassembly/emscripten-releases-builds/deps/${zipName}`;
  mkdirSync(join(root, ".work"), { recursive: true });
  const zip = join(root, ".work", zipName);
  console.log(`downloading ${url}`);
  runChecked(["curl.exe", "-fL", "--retry", "3", "-o", zip, url], root, envForEmsdk(), fail, "python download");
  mkdirSync(dest, { recursive: true });
  runChecked(["tar.exe", "-xf", zip, "-C", dest], root, envForEmsdk(), fail, "python unzip");
  rmSync(zip, { force: true });
  const exe = findNamed(dest, "python.exe", 3);
  if (!exe) fail(`python zip had no python.exe (${url})`);
  return exe;
}

function pythonForEmsdk(root: string, dir: string, fail: Fail): string {
  const bundled = findNamed(join(dir, "python"), "python.exe", 2);
  if (bundled) return bundled;
  for (const cmd of [["py", "-3"], ["python"], ["python3"]]) {
    const exe = python310(cmd);
    if (exe) return exe;
  }
  return bootstrapPython(root, fail);
}

function ensureWindowsEmsdk(root: string, fail: Fail): string {
  const dir = join(root, ".work", "emsdk");
  if (!isFile(join(dir, "emsdk.py"))) {
    if (existsSync(dir)) rmSync(dir, { recursive: true, force: true });
    const git = which("git");
    if (!git) fail("git not found. Install Git for Windows, then rerun the wasm build.");
    mkdirSync(join(root, ".work"), { recursive: true });
    console.log("cloning emsdk into .work/emsdk");
    runChecked(
      [git, "clone", "--depth", "1", "https://github.com/emscripten-core/emsdk.git", dir],
      root,
      envForEmsdk(),
      fail,
      "git clone emsdk",
    );
  }
  const python = pythonForEmsdk(root, dir, fail);
  if (!wasmTools(dir)) {
    console.log("installing emscripten sdk into .work/emsdk (first wasm build, this downloads the compiler)");
    runChecked([python, join(dir, "emsdk.py"), "install", "latest"], dir, envForEmsdk(), fail, "emsdk install latest");
  }
  if (!isFile(join(dir, ".emscripten"))) {
    // activate prints the whole PATH; keep that off the build log
    runChecked(
      [python, join(dir, "emsdk.py"), "activate", "latest"],
      dir,
      envForEmsdk(),
      fail,
      "emsdk activate latest",
      true,
    );
  }
  if (!wasmTools(dir)) fail(`emsdk install finished but em++ is missing in ${dir}`);
  return dir;
}

// `construct_env` writes emsdk_set_env.bat (`SET KEY=value`) and leaves it
// behind when emsdk.py is invoked directly. emsdk.bat would run and delete it.
function windowsEmsdkEnv(dir: string, python: string, fail: Fail): Record<string, string> {
  const r = Bun.spawnSync([python, join(dir, "emsdk.py"), "construct_env"], {
    cwd: dir,
    env: envForEmsdk(),
    stdout: "pipe",
    stderr: "pipe",
  });
  const script = join(dir, "emsdk_set_env.bat");
  if (r.exitCode !== 0 || !isFile(script)) {
    fail(`emsdk construct_env failed:\n${r.stderr.toString()}${r.stdout.toString()}`);
  }
  const text = readFileSync(script, "utf8");
  rmSync(script, { force: true });
  const env: Record<string, string> = {};
  for (const line of text.split(/\r?\n/)) {
    const m = /^(?:SET|set)\s+([A-Za-z_][A-Za-z0-9_]*)=(.*)$/.exec(line.trim());
    if (!m) continue;
    env[m[1]!] = m[2] ?? "";
  }
  if (!env["EMSDK"] || !env["PATH"]) fail("emsdk environment is missing EMSDK or PATH");
  env["PYTHONHOME"] = "";
  env["PYTHONPATH"] = "";
  return env;
}

function windowsWasmToolchain(root: string, fail: Fail): Toolchain {
  const dir = ensureWindowsEmsdk(root, fail);
  const tools = wasmTools(dir)!;
  const python = pythonForEmsdk(root, dir, fail);
  const env = windowsEmsdkEnv(dir, python, fail);
  return {
    plat: "wasm",
    cxx: tools.cxx,
    cc: tools.cc,
    ar: tools.ar,
    linker: tools.cxx,
    env,
    msvcStyle: false,
    gnu: false,
    label: "em++",
    objExt: "o",
  };
}

function findWasmToolchain(root: string, fail: Fail): Toolchain {
  if (process.platform === "win32") return windowsWasmToolchain(root, fail);
  let cxx = which("em++");
  let cc = which("emcc");
  let ar = which("emar");
  if (!cxx) {
    for (const r of emsdkRoots(root)) {
      const tools = wasmTools(r);
      if (!tools) continue;
      cxx = tools.cxx;
      cc = tools.cc;
      ar = tools.ar;
      break;
    }
  }
  if (!cxx || !cc || !ar) {
    fail(
      "em++ not found. Install emsdk (https://emscripten.org/docs/getting_started/downloads.html),\n" +
        "run `emsdk install latest && emsdk activate latest`, and set EMSDK or put it on PATH.",
    );
  }
  return { plat: "wasm", cxx, cc, ar, linker: cxx, env: {}, msvcStyle: false, gnu: false, label: "em++", objExt: "o" };
}

export function findToolchain(root: string, plat: Platform, clang: boolean, fail: (msg: string) => never): Toolchain {
  if (plat === "win") return findWindowsToolchain(root, clang, fail);
  if (plat === "wasm") return findWasmToolchain(root, fail);
  return findUnixToolchain(plat, clang, fail);
}
