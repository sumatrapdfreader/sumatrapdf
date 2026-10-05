// Helpers the code generators (cmd/ng-gen-settings.ts, cmd/ng-gen-commands.ts) need.
// In orig these live in cmd/util.ts next to the premake/ninja drivers, which we
// don't have; clang-format is located the way cmd/format.ts does it.

import { readFileSync } from "node:fs";
import { join } from "node:path";

export async function runLogged(cmd: string, args: string[], cwd?: string): Promise<void> {
  const short = [cmd.split("\\").pop(), ...args].join(" ");
  console.log(`> ${short}`);
  const proc = Bun.spawn([cmd, ...args], { cwd, stdout: "inherit", stderr: "inherit", stdin: "inherit" });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`command failed with exit code ${exitCode}`);
  }
}

export function findClangFormat(): string | null {
  const onPath = Bun.which("clang-format");
  if (onPath) return onPath;
  const pf = process.env["ProgramFiles"] ?? "C:\\Program Files";
  for (const v of ["18", "2026", "17", "2022"]) {
    for (const ed of ["Community", "Professional", "Enterprise", "BuildTools", "Preview"]) {
      const p = join(pf, "Microsoft Visual Studio", v, ed, "VC", "Tools", "Llvm", "x64", "bin", "clang-format.exe");
      if (Bun.file(p).size > 0) return p;
    }
  }
  return null;
}

// clang-format every generated C++ file so the output matches cmd/format.ts
export async function clangFormatFiles(rootDir: string, relativePaths: string[]): Promise<void> {
  const exe = findClangFormat();
  if (!exe) {
    throw new Error("couldn't find clang-format");
  }
  for (const rel of [...new Set(relativePaths)]) {
    const path = join(rootDir, rel);
    await runLogged(exe, ["-i", "-style=file", path]);
    // the repo uses LF; clang-format on Windows may leave CRLF behind
    const text = await Bun.file(path).text();
    const lf = text.replace(/\r\n/g, "\n");
    if (lf !== text) await Bun.write(path, lf);
  }
}

export async function isGitClean(dir: string): Promise<boolean> {
  const proc = Bun.spawn(["git", "status", "--porcelain"], { cwd: dir || ".", stdout: "pipe", stderr: "inherit" });
  const out = (await new Response(proc.stdout).text()).trim();
  await proc.exited;
  if (out.length > 0) {
    console.log(`git status --porcelain returned:\n'${out}'`);
  }
  return out.length === 0;
}

export function extractSumatraVersion(): string {
  const path = join(import.meta.dir, "..", "..", "src", "ng", "Version.h");
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
