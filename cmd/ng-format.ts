// Formats sources: clang-format for src/ng/**/*.{cpp,c,h}, prettier for cmd/**/*.ts.
//
//   bun cmd/ng-format.ts          # both
//   bun cmd/ng-format.ts -ts      # prettier only
//   bun cmd/ng-format.ts a.cpp b.h   # clang-format just these files

import { Glob } from "bun";
import { join } from "node:path";

const root = join(import.meta.dir, "..");

function findClangFormat(): string | null {
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

async function formatCpp(files: string[]): Promise<void> {
  const exe = findClangFormat();
  if (!exe) {
    console.error("clang-format not found (install LLVM or the VS C++ Clang tools)");
    process.exit(1);
  }
  if (files.length === 0) {
    const g = new Glob("src/ng/**/*.{cpp,c,h}");
    files = Array.from(g.scanSync({ cwd: root })).map((p) => p.replaceAll("\\", "/"));
  }
  console.log(`clang-format: ${files.length} files`);
  const batch = 50;
  for (let i = 0; i < files.length; i += batch) {
    const r = Bun.spawnSync([exe, "-i", "-style=file", ...files.slice(i, i + batch)], { cwd: root, stderr: "pipe" });
    if (r.exitCode !== 0) {
      console.error(r.stderr.toString());
      process.exit(1);
    }
  }
  // the repo uses LF; clang-format on Windows may leave CRLF behind
  for (const f of files) {
    const p = join(root, f);
    const text = await Bun.file(p).text();
    const lf = text.replace(/\r\n/g, "\n");
    if (lf !== text) await Bun.write(p, lf);
  }
}

function formatTs(): void {
  console.log("prettier: cmd/**/*.ts");
  const r = Bun.spawnSync(["bunx", "prettier", "--write", "--log-level", "warn", "cmd/**/*.ts"], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });
  if (r.exitCode !== 0) process.exit(1);
}

const args = process.argv.slice(2);
const tsOnly = args.includes("-ts");
const files = args.filter((a) => !a.startsWith("-"));
if (tsOnly) {
  formatTs();
} else if (files.length > 0) {
  await formatCpp(files);
} else {
  await formatCpp([]);
  formatTs();
}
