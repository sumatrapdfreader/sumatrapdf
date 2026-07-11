import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, dirname, extname } from "node:path";

const msBuildRelPath = String.raw`MSBuild\Current\Bin\MSBuild.exe`;
// VS 2022 ships llvm tools in Llvm\bin, VS 18 in Llvm\x64\bin
const clangFormatRelPaths = [String.raw`VC\Tools\Llvm\bin\clang-format.exe`, String.raw`VC\Tools\Llvm\x64\bin\clang-format.exe`];
const clangTidyRelPaths = [String.raw`VC\Tools\Llvm\bin\clang-tidy.exe`, String.raw`VC\Tools\Llvm\x64\bin\clang-tidy.exe`];

const vsEditions = ["Community", "Professional", "Enterprise"];

export interface VisualStudioInfo {
  vsRoot: string;
  msbuildPath: string;
  clangFormatPath: string;
  clangTidyPath: string;
  llvmPdbutilPath?: string;
}

// llvm-pdbutil.exe doesn't work when invoked using absolute path
// so it must be in %PATH% to work
function findLlvmPdbUtil(): string | undefined {
  try {
    const result = Bun.spawnSync(["llvm-pdbutil", "--help"], {
      stdout: "pipe",
      stderr: "pipe",
    });
    if (result.exitCode === 0) {
      return "llvm-pdbutil.exe";
    }
  } catch {}
  return;
}

function findVsRootVer(ver: string): string {
  // try PATH first
  try {
    const result = Bun.spawnSync(["msbuild", "-h"], {
      stdout: "pipe",
      stderr: "pipe",
    });
    if (result.exitCode === 0) {
      const pathEnv = process.env.PATH ?? "";
      for (const dir of pathEnv.split(";")) {
        if (!dir) continue;
        const p = join(dir, "MSBuild.exe");
        if (existsSync(p)) {
          // walk up from e.g. .../MSBuild/Current/Bin or .../MSBuild/Current/Bin/amd64
          // until we pass the "MSBuild" directory to get vsRoot
          let d = dir;
          while (d && d !== dirname(d)) {
            const base = d.split(/[\\/]/).pop()!;
            if (base.toLowerCase() === "msbuild") {
              return dirname(d);
            }
            d = dirname(d);
          }
        }
      }
    }
  } catch {
    // msbuild not in PATH
  }

  // try known Program Files locations
  const programFiles = process.env["ProgramFiles"] ?? String.raw`C:\Program Files`;
  const vsBase = join(programFiles, "Microsoft Visual Studio", ver);
  for (const edition of vsEditions) {
    const vsRoot = join(vsBase, edition);
    if (existsSync(join(vsRoot, msBuildRelPath))) {
      return vsRoot;
    }
  }
  return "";
}

function findTool(vsRoot: string, relPath: string): string {
  const p = join(vsRoot, relPath);
  if (existsSync(p)) {
    return p;
  }
  return "";
}

function findToolMulti(vsRoot: string, relPaths: string[]): string {
  for (const relPath of relPaths) {
    const p = findTool(vsRoot, relPath);
    if (p) {
      return p;
    }
  }
  return "";
}

export function detectVisualStudioVer(ver: string): VisualStudioInfo | undefined {
  const vsRoot = findVsRootVer(ver);
  if (vsRoot == "") return;

  const msbuildPath = findTool(vsRoot, msBuildRelPath);
  if (!msbuildPath) {
    return;
  }

  const clangFormatPath = findToolMulti(vsRoot, clangFormatRelPaths);
  const clangTidyPath = findToolMulti(vsRoot, clangTidyRelPaths);
  const llvmPdbutilPath = findLlvmPdbUtil();

  console.log(`vsRoot: ${vsRoot}`);
  console.log(`msbuildPath: ${msbuildPath}`);
  if (clangFormatPath) console.log(`clangFormatPath: ${clangFormatPath}`);
  if (clangTidyPath) console.log(`clangTidyPath: ${clangTidyPath}`);
  if (llvmPdbutilPath) console.log(`llvmPdbutilPath: ${llvmPdbutilPath}`);

  return { vsRoot, msbuildPath, clangFormatPath, clangTidyPath, llvmPdbutilPath };
}

export function detectVisualStudio2022(): VisualStudioInfo {
  let res = detectVisualStudioVer("2022");
  if (!res) {
    throw new Error(`couldn't find vs 2022 msbuild.exe `);
  }
  return res;
}

export function detectVisualStudio2026(): VisualStudioInfo {
  let res = detectVisualStudioVer("18");
  if (!res) {
    throw new Error(`couldn't find vs 2026 msbuild.exe `);
  }
  return res;
}

export function detectVisualStudio(): VisualStudioInfo {
  let res = detectVisualStudioVer("2022");
  if (res) return res;
  if (!res) {
    res = detectVisualStudioVer("18");
  }
  if (!res) {
    throw new Error(`couldn't find vs 2026 or vs 2022`);
  }
  return res;
}

export async function runLogged(cmd: string, args: string[], cwd?: string): Promise<void> {
  const short = [cmd.split("\\").pop(), ...args].join(" ");
  console.log(`> ${short}`);
  const proc = Bun.spawn([cmd, ...args], {
    cwd,
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`command failed with exit code ${exitCode}`);
  }
}

// clang-format every generated C++ file so gen-code output matches cmd/format.ts
export async function clangFormatFiles(rootDir: string, relativePaths: string[]): Promise<void> {
  const { clangFormatPath } = detectVisualStudio();
  if (!clangFormatPath) {
    throw new Error("couldn't find clang-format.exe");
  }
  for (const rel of [...new Set(relativePaths)]) {
    const path = join(rootDir, rel);
    await runLogged(clangFormatPath, ["-i", "-style=file", path]);
  }
}

export async function isGitClean(dir: string): Promise<boolean> {
  const proc = Bun.spawn(["git", "status", "--porcelain"], {
    cwd: dir || ".",
    stdout: "pipe",
    stderr: "inherit",
  });
  const out = (await new Response(proc.stdout).text()).trim();
  await proc.exited;
  if (out.length > 0) {
    console.log(`git status --porcelain returned:\n'${out}'`);
  }
  return out.length === 0;
}

export async function getGitSha1(): Promise<string> {
  const proc = Bun.spawn(["git", "rev-parse", "HEAD"], { stdout: "pipe", stderr: "inherit" });
  const s = (await new Response(proc.stdout).text()).trim();
  await proc.exited;
  if (s.length !== 40) throw new Error(`getGitSha1: '${s}' doesn't look like sha1`);
  return s;
}

export async function getGitLinearVersion(): Promise<number> {
  const proc = Bun.spawn(["git", "log", "--oneline"], { stdout: "pipe", stderr: "inherit" });
  const out = await new Response(proc.stdout).text();
  await proc.exited;
  const lines = out.split("\n").filter((l) => l.trim() !== "");
  const n = lines.length + 1000;
  if (n < 10000) throw new Error(`getGitLinearVersion: n is ${n} (should be > 10000)`);
  return n;
}

export function extractSumatraVersion(): string {
  const path = join("src", "Version.h");
  const content = readFileSync(path, "utf-8");
  const prefix = "#define CURR_VERSION ";
  for (const line of content.split("\n")) {
    if (line.startsWith(prefix)) {
      const ver = line.substring(prefix.length).trim();
      const parts = ver.split(".");
      if (parts.length === 0 || parts.length > 3) throw new Error(`invalid version: ${ver}`);
      for (const p of parts) {
        if (isNaN(parseInt(p, 10))) throw new Error(`invalid version: ${ver}`);
      }
      return ver;
    }
  }
  throw new Error(`couldn't extract CURR_VERSION from ${path}`);
}

function normalizeNewlines(data: string): string {
  return data.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}
const extsToNormalizeNL = [".md", ".css"];
export function copyFileNormalized(dst: string, src: string): void {
  const dstDir = join(dst, "..");
  mkdirSync(dstDir, { recursive: true });
  const ext = extname(dst).toLowerCase();
  if (extsToNormalizeNL.includes(ext)) {
    const data = readFileSync(src, "utf-8");
    writeFileSync(dst, normalizeNewlines(data));
  } else {
    copyFileSync(src, dst);
  }
}
