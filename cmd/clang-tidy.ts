import { Glob } from "bun";
import { readdirSync, existsSync } from "node:fs";
import { unlink, appendFile, writeFile } from "node:fs/promises";
import { join, basename } from "node:path";
import { detectVisualStudio } from "./util";

// Keep in sync with AdditionalIncludeDirectories in vs2022/SumatraPDF-static.vcxproj.
// A missing dir isn't fatal: clang reports "file not found" and keeps parsing with
// that header's declarations unknown, which silently degrades every check in the
// files that include it (this used to affect 25 files, SumatraPDF.cpp among them).
const includeDirs = [
  "ext/mupdf/include",
  "src",
  "src/base",
  "src/gui",
  "src/gui/win",
  "ext/chmdec",
  "ext/djvudec",
  "ext/a-zlib",
  "ext/synctex",
  "ext/lzma/C",
  "ext/libwebp/src",
  "ext/freetype/include",
  "ext/libarchive",
  "ext/a-zopfli",
  "ext/cmark-gfm/src",
  "ext/cmark-gfm/extensions",
  "ext/mupdf/scripts/cmark-gfm",
  "ext/heicdec",
  "ext/jxldec",
  "ext/darkmodelib/include",
];

// resolved at runtime so bumping the NuGet package doesn't silently re-break WebView.cpp
function webView2IncludeDir(): string | null {
  const packages = "packages";
  if (!existsSync(packages)) {
    return null;
  }
  const dir = readdirSync(packages).find((d) => d.startsWith("Microsoft.Web.WebView2."));
  if (!dir) {
    return null;
  }
  const inc = join(packages, dir, "build/native/include");
  return existsSync(inc) ? inc : null;
}

function buildIncludes(): string[] {
  const dirs = [...includeDirs];
  const wv2 = webView2IncludeDir();
  if (wv2) {
    dirs.push(wv2);
  } else {
    console.log("warning: WebView2 package not found, WebView.cpp will not fully parse");
  }
  for (const d of dirs) {
    if (!existsSync(d)) {
      console.log(`warning: include dir '${d}' does not exist`);
    }
  }
  return dirs.flatMap((d) => ["-I", d]);
}

const includes = buildIncludes();

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

// Checks, HeaderFilterRegex/ExcludeHeaderFilterRegex and FormatStyle all come from
// .clang-tidy -- don't pass --checks/--header-filter here or that file stops being
// the single source of truth. -fix only adds --fix on top.
function clangTidyFileArgs(path: string): string[] {
  return ["-extra-arg=-std=c++20", path, "--", ...includes, ...defines];
}

function clangTidyFixArgs(path: string): string[] {
  return ["--fix", "-extra-arg=-std=c++20", path, "--", ...includes, ...defines];
}

const whitelisted = [
  "resource.h",
  "version.h",
  // vendored PCH shim: a lone #include "hb.hh", needs harfbuzz-only include dirs
  "harfbuzzpch.cpp",
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
    "src/gui/*.cpp",
    "src/gui/win/*.cpp",
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
