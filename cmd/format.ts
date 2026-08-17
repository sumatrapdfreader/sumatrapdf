import { $, Glob } from "bun";
import { basename } from "node:path";
import { cpus } from "node:os";
import { detectVisualStudio } from "./util";

async function globFiles(patterns: string[]): Promise<string[]> {
  const files: string[] = [];
  for (const pattern of patterns) {
    const glob = new Glob(pattern);
    for await (const file of glob.scan({ dot: false })) {
      files.push(file);
    }
  }
  return files;
}

const whitelisted = ["resource.h", "version.h", "translationlangs.cpp"];

function isWhitelisted(path: string): boolean {
  const name = basename(path).toLowerCase();
  return whitelisted.includes(name);
}

// Repo uses LF; clang-format on Windows may leave or introduce CRLF.
function ensureLf(text: string): string {
  return text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}

async function formatFile(clangFormatPath: string, path: string): Promise<void> {
  await $`${clangFormatPath} -i -style=file ${path}`.quiet();
  const text = await Bun.file(path).text();
  const lf = ensureLf(text);
  if (lf !== text) {
    await Bun.write(path, lf);
  }
}

// prettier owns our .ts sources; .prettierrc.json / .prettierignore have the settings
const prettierGlobs = ["cmd/**/*.ts", "tests/**/*.ts"];

async function formatTsFiles(): Promise<void> {
  console.log(`running prettier on ${prettierGlobs.join(" ")}`);
  await $`bunx prettier --write --log-level warn ${prettierGlobs}`;
}

function usage(): string {
  return `Usage: bun cmd/format.ts [-ts]

  (no options)  format C/C++ sources with clang-format and .ts sources with prettier
  -ts           only run prettier on the .ts sources
`;
}

async function main() {
  const args = process.argv.slice(2);
  const tsOnly = args.includes("-ts");
  const unknown = args.filter((a) => a !== "-ts");
  if (unknown.length > 0) {
    console.error(`unknown option: ${unknown[0]}\n`);
    console.error(usage());
    process.exit(1);
  }

  await formatTsFiles();
  if (tsOnly) {
    return;
  }

  const { clangFormatPath: cfPath } = detectVisualStudio();
  const clangFormatPath = cfPath || "clang-format.exe";
  console.log(`using '${clangFormatPath}'`);

  const patterns = [
    "src/*.cpp",
    "src/*.h",
    "src/mui/*.cpp",
    "src/mui/*.h",
    "src/base/*.cpp",
    "src/base/*.h",
    "src/base/tests/*.cpp",
    "src/base/tests/*.h",
    "src/gui/*",
    "src/gui/win/*",
    "src/uia/*",
    "src/tools/**/*.{cpp,c,h,hpp}",
    "src/ifilter/*.cpp",
    "src/ifilter/*.h",
    "src/previewer/*.cpp",
    "src/previewer/*.h",
    "ext/mupdf_load_system_font.c",
  ];

  const files = await globFiles(patterns);
  const toFormat = files.filter((f) => !isWhitelisted(f));

  const concurrency = cpus().length;
  let i = 0;

  async function next(): Promise<void> {
    while (i < toFormat.length) {
      const idx = i++;
      await formatFile(clangFormatPath, toFormat[idx]);
    }
  }

  const workers = Array.from({ length: Math.min(concurrency, toFormat.length) }, () => next());
  await Promise.all(workers);

  console.log(`formatted ${toFormat.length} files`);
  console.log(`used '${clangFormatPath}'`);
}

await main();
