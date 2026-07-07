// Systematically test whether #include lines in mupdf .c sources are needed.
// File list comes from mupdf_files() in premake5.files.lua; compile flags match
// the mupdf project (Debug|x64) from premake5.lua.
//
// Usage:
//   bun cmd/remove-unused-mupdf-includes.ts
//   bun cmd/remove-unused-mupdf-includes.ts --file mupdf/source/fitz/error.c
//   bun cmd/remove-unused-mupdf-includes.ts --from 42
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { cpus } from "node:os";
import { basename, dirname, join } from "node:path";

const premakeFilesPath = join("premake5.files.lua");
const objDir = join("out", "dbg64", "obj-s", "mupdf", "remove-unused");

interface IncludeLine {
  lineIndex: number;
  include: string;
}

interface TestJob {
  n: number;
  file: string;
  include: IncludeLine;
  tempC: string;
  objFile: string;
}

interface RemovedInclude {
  file: string;
  lineIndex: number;
  include: string;
}

// mupdf project, Debug|x64 (matches premake mixed_dbg_rel_conf + mupdf defines)
const clArgs = [
  "/nologo",
  "/c",
  "/W4",
  "/wd4127",
  "/wd4189",
  "/wd4324",
  "/wd4458",
  "/wd4522",
  "/wd4611",
  "/wd4702",
  "/wd4800",
  "/wd6319",
  "/wd4005",
  "/wd4013",
  "/wd4018",
  "/wd4057",
  "/wd4100",
  "/wd4115",
  "/wd4130",
  "/wd4132",
  "/wd4146",
  "/wd4200",
  "/wd4204",
  "/wd4206",
  "/wd4210",
  "/wd4245",
  "/wd4267",
  "/wd4295",
  "/wd4305",
  "/wd4389",
  "/wd4456",
  "/wd4457",
  "/wd4703",
  "/wd4706",
  "/wd4819",
  "/wd5286",
  "/DWIN32",
  "/D_WIN32",
  "/DWINVER=0x0601",
  "/D_WIN32_WINNT=0x0601",
  "/DNTDDI_VERSION=0x06010000",
  "/D_HAS_ITERATOR_DEBUGGING=0",
  "/DDEBUG",
  "/DUSE_JPIP",
  "/DOPJ_EXPORTS",
  "/DHAVE_LCMS2MT=1",
  "/DOPJ_STATIC",
  "/DSHARE_JPEG",
  "/DTOFU_NOTO",
  "/DTOFU_CJK_LANG",
  "/DTOFU_NOTO_SUMATRA",
  "/DFZ_ENABLE_PDF=1",
  "/DFZ_ENABLE_SVG=1",
  "/DFZ_ENABLE_BROTLI=1",
  "/DFZ_ENABLE_BARCODE=0",
  "/DFZ_ENABLE_JS=1",
  "/DFZ_ENABLE_HYPHEN=0",
  "/DFZ_ENABLE_MD=1",
  "/DHAVE_LIBARCHIVE",
  "/DLIBARCHIVE_STATIC",
  "/D_UCRT_NOISY_NAN",
  "/DCMARK_GFM_STATIC_DEFINE",
  "/D_HAS_EXCEPTIONS=0",
  "/I",
  "ext/zlib",
  "/I",
  "mupdf/include",
  "/I",
  "mupdf/generated",
  "/I",
  "ext/jbig2dec",
  "/I",
  "ext/libjpeg-turbo",
  "/I",
  "ext/openjpeg/src/lib/openjp2",
  "/I",
  "mupdf/scripts/freetype",
  "/I",
  "ext/freetype/include",
  "/I",
  "ext/mujs",
  "/I",
  "ext/brotli/c/include",
  "/I",
  "ext/cmark-gfm/src",
  "/I",
  "ext/cmark-gfm/extensions",
  "/I",
  "mupdf/scripts/cmark-gfm",
  "/I",
  "ext/harfbuzz/src",
  "/I",
  "ext/lcms2/include",
  "/I",
  "ext/gumbo-parser/src",
  "/I",
  "ext/extract/include",
  "/I",
  "ext/libarchive",
  "/Od",
  "/MT",
  "/EHs-c-",
  "/GR-",
  "/utf-8",
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
      console.log(`Usage: bun cmd/remove-unused-mupdf-includes.ts [--file <path>] [--from <n>]`);
      console.log(`  --file   only process this .c file (e.g. mupdf/source/fitz/error.c)`);
      console.log(`  --from   start at include number n (1-based, for resuming)`);
      process.exit(0);
    }
    throw new Error(`unknown argument: ${arg}`);
  }
  return { fileFilter, from };
}

function extractQuotedCFiles(block: string, dir?: string): string[] {
  const result: string[] = [];
  for (const line of block.split("\n")) {
    const trimmed = line.trim();
    if (trimmed.startsWith("--")) {
      continue;
    }
    const m = trimmed.match(/"([^"]+)"/);
    if (!m) {
      continue;
    }
    const path = m[1];
    if (!path.endsWith(".c")) {
      continue;
    }
    result.push(dir ? `${dir}/${path}` : path.replace(/\\/g, "/"));
  }
  return result;
}

function mupdfCFilesFromPremake(): string[] {
  const lua = readFileSync(premakeFilesPath, "utf-8");
  const start = lua.indexOf("function mupdf_files()");
  const end = lua.indexOf("\nend\n\nfunction synctex_files()", start);
  if (start < 0 || end < 0) {
    throw new Error("could not find mupdf_files() in premake5.files.lua");
  }
  const body = lua.slice(start, end);

  const files: string[] = [];
  const dirBlockRe = /files_in_dir\(\s*"([^"]+)"\s*,\s*\{([\s\S]*?)\}\s*\)/g;
  let m: RegExpExecArray | null;
  while ((m = dirBlockRe.exec(body)) !== null) {
    files.push(...extractQuotedCFiles(m[2], m[1].replace(/\\/g, "/")));
  }

  const filesBlockRe = /files\s*\{\s*([\s\S]*?)\}/g;
  while ((m = filesBlockRe.exec(body)) !== null) {
    files.push(...extractQuotedCFiles(m[1]));
  }

  const unique = [...new Set(files)];
  unique.sort();
  return unique;
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

function tempCPath(n: number, file: string): string {
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
      const tempC = tempCPath(n, file);
      jobs.push({
        n,
        file,
        include,
        tempC,
        objFile: join(objDir, `${n}-${basename(file, ".c")}.obj`),
      });
      writeFileSync(tempC, removeLine(content, include.lineIndex), "utf-8");
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

async function compileC(cFile: string, objFile: string): Promise<boolean> {
  const pdbFile = objFile.replace(/\.obj$/i, ".pdb");
  const proc = Bun.spawn(["cl", ...clArgs, `/Fo${objFile}`, `/Fd${pdbFile}`, cFile], {
    stdout: "pipe",
    stderr: "pipe",
  });
  await Promise.all([new Response(proc.stdout).text(), new Response(proc.stderr).text()]);
  await proc.exited;
  return proc.exitCode === 0;
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
  const parallelism = cpus().length;

  let files = mupdfCFilesFromPremake();
  if (fileFilter) {
    const normalized = fileFilter.replace(/\\/g, "/");
    files = files.filter((f) => f === normalized);
    if (files.length === 0) {
      throw new Error(`--file ${fileFilter} is not a mupdf .c source from premake mupdf_files()`);
    }
  }

  mkdirSync(objDir, { recursive: true });

  console.log("writing temp .c files...");
  const jobs = buildJobs(files, from);
  console.log(`${files.length} .c files, ${jobs.length} #include lines to test, ${parallelism} workers`);
  if (jobs.length === 0) {
    return;
  }

  const warmup = await compileC(jobs[0].file, join(objDir, "warmup.obj"));
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
    const ok = await compileC(job.tempC, job.objFile);
    deleteFile(job.tempC);

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
}

await main();