import { Glob } from "bun";
import { unlink, appendFile, writeFile } from "node:fs/promises";
import { join, basename } from "node:path";
import { detectVisualStudio } from "./util";

const includes = [
  "-I",
  "mupdf/include",
  "-I",
  "src",
  "-I",
  "src/base",
  "-I",
  "src/wingui",
  "-I",
  "ext/libchm",
  "-I",
  "ext/libdjvu",
  "-I",
  "ext/zlib",
  "-I",
  "ext/synctex",
  "-I",
  "ext/lzma/C",
  "-I",
  "ext/libwebp/src",
  "-I",
  "ext/freetype/include",
];

const defines = [
  "-DUNICODE",
  "-DWIN32",
  "-D_WIN32",
  "-D_CRT_SECURE_NO_WARNINGS",
  "-DWINVER=0x0a00",
  "-D_WIN32_WINNT=0x0a00",
  "-DPRE_RELEASE_VER=3.3",
];

const clangTidyLogFile = "clangtidy.out.txt";

async function runAndLog(exePath: string, args: string[]): Promise<void> {
  const proc = Bun.spawn([exePath, ...args], {
    stdout: "pipe",
    stderr: "pipe",
  });

  async function pipeStream(stream: ReadableStream<Uint8Array>, out: { write(chunk: Uint8Array): void }) {
    const reader = stream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      out.write(value);
      await appendFile(clangTidyLogFile, value);
    }
  }

  console.log(`> ${basename(exePath)} ${args.join(" ")}`);
  await Promise.all([pipeStream(proc.stdout, process.stdout), pipeStream(proc.stderr, process.stderr)]);

  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    console.error(`clang-tidy exited with code ${exitCode}`);
    process.exit(exitCode);
  }
}

function clangTidyFileArgs(path: string): string[] {
  return ["--header-filter=.*", "-extra-arg=-std=c++20", path, "--", ...includes, ...defines];
}

function clangTidyFixArgs(path: string): string[] {
  return [
    "--checks=-*,modernize-raw-string-literal",
    "-p",
    ".",
    "--header-filter=src/",
    "--fix",
    "-extra-arg=-std=c++20",
    path,
    "--",
    ...includes,
    ...defines,
  ];
}

const whitelisted = [
  "resource.h",
  "version.h",
  "translationlangs.cpp",
  "doc.cpp",
  "ebookcontroller.cpp",
  "ebookcontrols.cpp",
  "ebookformatter.cpp",
  "engineebook.cpp",
  "htmlformatter.cpp",
  "stresstesting.cpp",
  "tester.cpp",
];

function isWhitelisted(path: string): boolean {
  const name = basename(path).toLowerCase();
  if (name.endsWith(".h")) return true;
  return whitelisted.includes(name);
}

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

async function main() {
  const args = process.argv.slice(2);
  const fix = args.includes("-fix");

  const { clangTidyPath } = detectVisualStudio();
  const exePath = clangTidyPath || "clang-tidy.exe";
  console.log(`using '${exePath}'`);

  await unlink(clangTidyLogFile).catch(() => {});
  await writeFile(clangTidyLogFile, "");

  const patterns = [
    "src/*.cpp",
    "src/mui/*.cpp",
    "src/base/*.cpp",
    "src/wingui/*.cpp",
    "src/uia/*.cpp",
    "src/previewer/*.cpp",
    "src/ifilter/*.cpp",
  ];

  const files = await globFiles(patterns);
  const toProcess = files.filter((f) => !isWhitelisted(f));

  for (const file of toProcess) {
    const tidyArgs = fix ? clangTidyFixArgs(file) : clangTidyFileArgs(file);
    await runAndLog(exePath, tidyArgs);
  }

  console.log(`\nLogged output to '${clangTidyLogFile}'`);
}

await main();
