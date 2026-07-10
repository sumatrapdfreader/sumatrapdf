// Systematically test whether #include lines in SumatraPDF-dll .cpp sources are needed.
// For each include: write a temp .cpp with that include removed, compile with cl.exe
// (Release|Win32 flags) in parallel, then apply removals that compile successfully.
//
// Usage:
//   bun cmd/remove-unused-includes.ts
//   bun cmd/remove-unused-includes.ts --file src/Canvas.cpp
//   bun cmd/remove-unused-includes.ts --from 42
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { cpus } from "node:os";
import { basename, dirname, join } from "node:path";
import { detectVisualStudio2026 } from "./util";

const vcxprojPath = join("vs2022", "SumatraPDF-dll.vcxproj");
const objDir = join("out", "rel32", "obj", "x86", "Release", "SumatraPDF-dll", "remove-unused");

interface IncludeLine {
  lineIndex: number;
  include: string;
}

interface TestJob {
  n: number;
  file: string;
  include: IncludeLine;
  tempCpp: string;
  objFile: string;
}

interface RemovedInclude {
  file: string;
  lineIndex: number;
  include: string;
}

const clArgs = [
  "/nologo",
  "/c",
  "/W4",
  "/WX",
  "/wd4127",
  "/wd4189",
  "/wd4324",
  "/wd4458",
  "/wd4522",
  "/wd4611",
  "/wd4800",
  "/wd6319",
  "/wd4100",
  "/wd4244",
  "/wd4267",
  "/wd4701",
  "/wd4702",
  "/wd4703",
  "/wd4706",
  "/wd6324",
  "/wd4302",
  "/wd4311",
  "/wd4838",
  "/wd4819",
  "/DWIN32",
  "/D_WIN32",
  "/DWINVER=0x0601",
  "/D_WIN32_WINNT=0x0601",
  "/DNTDDI_VERSION=0x06010000",
  "/D_HAS_ITERATOR_DEBUGGING=0",
  "/DNDEBUG",
  "/DLIBARCHIVE_STATIC",
  "/D_DARKMODELIB_NO_INI_CONFIG",
  "/D_CRT_SECURE_NO_WARNINGS",
  "/DDISABLE_DOCUMENT_RESTRICTIONS",
  "/D_HAS_EXCEPTIONS=0",
  "/DUNICODE",
  "/D_UNICODE",
  "/I",
  "src",
  "/I",
  "mupdf/include",
  "/I",
  "ext/libdjvu",
  "/I",
  "ext/libchm",
  "/I",
  "ext/libarchive",
  "/I",
  "ext/darkmodelib/include",
  "/I",
  "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/include",
  "/I",
  "ext/zlib",
  "/I",
  "ext/synctex",
  "/O1",
  "/Gy",
  "/Oi",
  "/GF",
  "/MT",
  "/EHs-c-",
  "/GR-",
  "/utf-8",
  "/std:c++latest",
  "/external:W3",
  "/Zi",
];

function parseArgs(): { fileFilter?: string; from: number } {
  const args = process.argv.slice(2);
  let fileFilter: string | undefined;
  let from = 1;
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === "--file" && i + 1 < args.length) {
      fileFilter = args[++i].replace(/\\/g, "/");
      continue;
    }
    if (arg === "--from" && i + 1 < args.length) {
      from = parseInt(args[++i], 10);
      if (!Number.isFinite(from) || from < 1) {
        throw new Error(`invalid --from value: ${args[i]}`);
      }
      continue;
    }
    if (arg === "--help" || arg === "-h") {
      console.log(`Usage: bun cmd/remove-unused-includes.ts [--file <path>] [--from <n>]`);
      console.log(`  --file   only process this .cpp file (e.g. src/Canvas.cpp)`);
      console.log(`  --from   start at include number n (1-based, for resuming)`);
      process.exit(0);
    }
    throw new Error(`unknown argument: ${arg}`);
  }
  return { fileFilter, from };
}

function dllCppFiles(): string[] {
  const xml = readFileSync(vcxprojPath, "utf-8");
  const re = /ClCompile Include="\.\.\\(src\\[^"]+\.cpp)"/g;
  const files: string[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(xml)) !== null) {
    files.push(m[1].replace(/\\/g, "/"));
  }
  files.sort();
  return files;
}

function fileEol(content: string): string {
  return content.includes("\r\n") ? "\r\n" : "\n";
}

function splitLines(content: string): string[] {
  return content.split(/\r?\n/);
}

function joinLines(lines: string[], eol: string): string {
  return lines.join(eol);
}

function findIncludeLines(content: string): IncludeLine[] {
  const lines = splitLines(content);
  const result: IncludeLine[] = [];
  for (let i = 0; i < lines.length; i++) {
    const m = lines[i].match(/^\s*#\s*include\s+[<"]([^>"]+)[>"]/);
    if (m) {
      result.push({ lineIndex: i, include: m[1] });
    }
  }
  return result;
}

function removeLine(content: string, lineIndex: number): string {
  const eol = fileEol(content);
  const lines = splitLines(content);
  lines.splice(lineIndex, 1);
  return joinLines(lines, eol);
}

function removeLines(content: string, lineIndices: number[]): string {
  let result = content;
  for (const idx of [...lineIndices].sort((a, b) => b - a)) {
    result = removeLine(result, idx);
  }
  return result;
}

function tempCppPath(n: number, file: string): string {
  return join(dirname(file), `${n}-${basename(file)}`);
}

function buildJobs(files: string[], from: number): TestJob[] {
  const jobs: TestJob[] = [];
  let n = 0;
  for (const file of files) {
    const content = readFileSync(file, "utf-8");
    for (const include of findIncludeLines(content)) {
      n++;
      if (n < from) {
        continue;
      }
      const tempCpp = tempCppPath(n, file);
      jobs.push({
        n,
        file,
        include,
        tempCpp,
        objFile: join(objDir, `${n}-${basename(file, ".cpp")}.obj`),
      });
      writeFileSync(tempCpp, removeLine(content, include.lineIndex), "utf-8");
    }
  }
  return jobs;
}

function deleteFile(path: string): void {
  try {
    if (existsSync(path)) {
      unlinkSync(path);
    }
  } catch {}
}

async function compileCpp(cppFile: string, objFile: string): Promise<boolean> {
  const pdbFile = objFile.replace(/\.obj$/i, ".pdb");
  const proc = Bun.spawn(["cl", ...clArgs, `/Fo${objFile}`, `/Fd${pdbFile}`, cppFile], {
    stdout: "pipe",
    stderr: "pipe",
  });
  await Promise.all([new Response(proc.stdout).text(), new Response(proc.stderr).text()]);
  await proc.exited;
  return proc.exitCode === 0;
}

async function formatFile(clangFormatPath: string, path: string): Promise<void> {
  const proc = Bun.spawn([clangFormatPath, "-i", "-style=file", path], {
    stdout: "ignore",
    stderr: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`clang-format failed for ${path}`);
  }
}

async function runParallel<T>(items: T[], parallelism: number, fn: (item: T) => Promise<void>): Promise<void> {
  let next = 0;
  async function worker(): Promise<void> {
    while (true) {
      const i = next++;
      if (i >= items.length) {
        return;
      }
      await fn(items[i]);
    }
  }
  await Promise.all(Array.from({ length: parallelism }, () => worker()));
}

async function main() {
  const { fileFilter, from } = parseArgs();
  const { clangFormatPath } = detectVisualStudio2026();
  const parallelism = cpus().length;

  let files = dllCppFiles();
  if (fileFilter) {
    const normalized = fileFilter.replace(/\\/g, "/");
    files = files.filter((f) => f === normalized);
    if (files.length === 0) {
      throw new Error(`--file ${fileFilter} is not a SumatraPDF-dll .cpp source`);
    }
  }

  mkdirSync(objDir, { recursive: true });

  console.log("writing temp .cpp files...");
  const jobs = buildJobs(files, from);
  console.log(`${jobs.length} #include lines to test, ${parallelism} workers`);
  if (jobs.length === 0) {
    return;
  }

  const warmup = await compileCpp(jobs[0].file, join(objDir, "warmup.obj"));
  if (!warmup) {
    throw new Error(`initial compile of ${jobs[0].file} failed; fix the tree before running`);
  }

  const removed: RemovedInclude[] = [];
  let completed = 0;
  const total = jobs.length;
  const printLock = { chain: Promise.resolve() };

  function printLine(line: string): void {
    printLock.chain = printLock.chain.then(() => {
      console.log(line);
    });
  }

  await runParallel(jobs, parallelism, async (job) => {
    const ok = await compileCpp(job.tempCpp, job.objFile);
    deleteFile(job.tempCpp);

    completed++;
    const displayInclude = `#include "${job.include.include}"`;
    let line = `${completed} / ${total} ${job.file} ${displayInclude}`;
    if (ok) {
      removed.push({ file: job.file, lineIndex: job.include.lineIndex, include: job.include.include });
      line += " removed (not needed)";
    }
    printLine(line);
  });
  await printLock.chain;

  const byFile = new Map<string, RemovedInclude[]>();
  for (const item of removed) {
    let list = byFile.get(item.file);
    if (!list) {
      list = [];
      byFile.set(item.file, list);
    }
    list.push(item);
  }

  for (const [file, items] of byFile) {
    const content = readFileSync(file, "utf-8");
    const updated = removeLines(
      content,
      items.map((i) => i.lineIndex),
    );
    writeFileSync(file, updated, "utf-8");
  }

  console.log("");
  console.log(`=== summary: ${removed.length} unused #include(s) removed ===`);
  if (removed.length === 0) {
    console.log("(none)");
  } else {
    let prevFile = "";
    for (const item of removed.sort((a, b) => a.file.localeCompare(b.file) || a.lineIndex - b.lineIndex)) {
      if (item.file !== prevFile) {
        if (prevFile !== "") {
          console.log("");
        }
        console.log(`${item.file}:`);
        prevFile = item.file;
      }
      console.log(`  #include "${item.include}"`);
    }
  }

  if (byFile.size > 0 && clangFormatPath) {
    console.log("");
    console.log(`clang-format on ${byFile.size} modified file(s)...`);
    for (const file of byFile.keys()) {
      await formatFile(clangFormatPath, file);
    }
  }
}

await main();