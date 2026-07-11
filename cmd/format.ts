import { $, Glob } from "bun";
import { join, basename } from "node:path";
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

async function formatFile(clangFormatPath: string, path: string): Promise<void> {
  await $`${clangFormatPath} -i -style=file ${path}`.quiet();
}

async function main() {
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
    "src/wingui/*",
    "src/uia/*",
    "src/tools/*",
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
