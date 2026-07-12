import {
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

type Args = {
  repo: string;
  rev: string;
  keep: boolean;
};

const defaultRepo = "git://sourceware.org/git/bzip2.git";
const defaultRev = "bzip2-1.0.8";
const homepage = "https://www.sourceware.org/bzip2/";

const depsDir = "deps";
const checkoutDir = join(depsDir, "bzip2");
const outDir = join("ext", "a-bzip2");
const tmpDir = join("cmd", "tmp", "a-bzip2");

const sourcePreamble = `#ifndef BZ_NO_STDIO
#define BZ_NO_STDIO
#endif
#include "bzlib.h"
`;

const sourceFiles = [
  "blocksort.c",
  "bzlib.c",
  "compress.c",
  "crctable.c",
  "decompress.c",
  "huffman.c",
  "randtable.c",
];

const sumatraSourceAdditions = `
#include <assert.h>

void bz_internal_error(int errcode) {
  (void)errcode;
  assert(0);
}
`;

function usage(): never {
  console.error(`Usage: bun cmd/a-bzip2.ts [bzip2-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out bzip2 under deps/bzip2, generates ext/a-bzip2/bzlib.h and
ext/a-bzip2/bzip2.c, validates the amalgamated C file with cl.exe, and leaves
the generated files ready for the libarchive build.

Defaults:
  repo: ${defaultRepo}
  rev:  ${defaultRev}
`);
  process.exit(1);
}

function parseArgs(): Args {
  const args = process.argv.slice(2);
  if (args.includes("-h") || args.includes("--help")) {
    usage();
  }
  const positional = args.filter((arg) => arg !== "--keep");
  if (positional.length > 2) {
    usage();
  }
  return {
    repo: positional[0] ?? defaultRepo,
    rev: positional[1] ?? defaultRev,
    keep: args.includes("--keep"),
  };
}

async function checkout(
  repo: string,
  rev: string,
  keep: boolean,
): Promise<void> {
  mkdirSync(depsDir, { recursive: true });
  if (!keep && existsSync(checkoutDir)) {
    rmSync(checkoutDir, { recursive: true, force: true });
  }

  if (!existsSync(checkoutDir)) {
    await runLogged("git", ["clone", repo, checkoutDir]);
  }

  await runLogged("git", ["-C", checkoutDir, "fetch", "--tags", "--force"]);
  await runLogged("git", ["-C", checkoutDir, "checkout", "--force", rev]);
}

function readText(path: string): string {
  return readFileSync(path, "utf-8").replace(/\r\n/g, "\n");
}

function gitOutput(args: string[], cwd: string): string {
  const proc = Bun.spawnSync(["git", ...args], {
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  });
  if (proc.exitCode !== 0) {
    throw new Error(proc.stderr.toString().trim());
  }
  return proc.stdout.toString().trim();
}

function stripComments(text: string): string {
  let out = "";
  let i = 0;
  let state: "code" | "line" | "block" | "string" | "char" = "code";
  while (i < text.length) {
    const c = text[i];
    const n = text[i + 1];
    if (state === "code") {
      if (c === "/" && n === "/") {
        state = "line";
        i += 2;
        continue;
      }
      if (c === "/" && n === "*") {
        state = "block";
        i += 2;
        continue;
      }
      if (c === '"') {
        state = "string";
      } else if (c === "'") {
        state = "char";
      }
      out += c;
      i++;
      continue;
    }
    if (state === "line") {
      if (c === "\n") {
        out += "\n";
        state = "code";
      }
      i++;
      continue;
    }
    if (state === "block") {
      if (c === "\n") {
        out += "\n";
      }
      if (c === "*" && n === "/") {
        state = "code";
        i += 2;
      } else {
        i++;
      }
      continue;
    }
    out += c;
    if ((state === "string" || state === "char") && c === "\\") {
      out += n ?? "";
      i += 2;
      continue;
    }
    if (state === "string" && c === '"') {
      state = "code";
    } else if (state === "char" && c === "'") {
      state = "code";
    }
    i++;
  }
  return out;
}

function normalizeBlankLines(text: string): string {
  return (
    text
      .replace(/[ \t]+$/gm, "")
      .replace(/\n{3,}/g, "\n\n")
      .trim() + "\n"
  );
}

function listFiles(dir: string): string[] {
  return readdirSync(dir)
    .map((name) => join(dir, name))
    .filter((path) => statSync(path).isFile());
}

function findBzip2SrcDir(root: string): string {
  if (existsSync(join(root, "bzlib.h"))) {
    return root;
  }
  for (const path of listFiles(root)) {
    if (basename(path) === "bzlib.h") {
      return root;
    }
  }
  throw new Error(`could not find bzlib.h under ${root}`);
}

function removeLocalIncludes(text: string): string {
  return text
    .split("\n")
    .filter((line) => !/^\s*#\s*include\s+"[^"]+"/.test(line))
    .join("\n");
}

function prepareChunk(path: string): string {
  return normalizeBlankLines(
    removeLocalIncludes(stripComments(readText(path))),
  );
}

function generateAmalgamation(root: string): {
  header: string;
  source: string;
} {
  const srcDir = findBzip2SrcDir(root);
  const header = prepareChunk(join(srcDir, "bzlib.h"));
  const chunks: string[] = [
    sourcePreamble,
    prepareChunk(join(srcDir, "bzlib_private.h")),
  ];
  for (const name of sourceFiles) {
    chunks.push(prepareChunk(join(srcDir, name)));
  }
  chunks.push(normalizeBlankLines(sumatraSourceAdditions));
  return { header, source: normalizeBlankLines(chunks.join("\n")) };
}

function versionText(repo: string, rev: string): string {
  const commitSha1 = gitOutput(["rev-parse", "HEAD"], checkoutDir);
  return (
    [
      `project_homepage: ${homepage}`,
      `repo_url: ${repo}`,
      `revision: ${rev}`,
      `commit_sha1: ${commitSha1}`,
    ].join("\n") + "\n"
  );
}

async function validateCompile(header: string, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFileSync(join(tmpDir, "bzlib.h"), header);
  writeFileSync(join(tmpDir, "bzip2.c"), source);

  detectVisualStudio2026();
  await runLogged(
    "cl.exe",
    [
      "/nologo",
      "/c",
      "/W4",
      "/WX",
      "/O2",
      "/MT",
      "/D",
      "WIN32",
      "/D",
      "_WIN32",
      "/D",
      "NDEBUG",
      "/D",
      "BZ_NO_STDIO",
      "/D",
      "_CRT_SECURE_NO_WARNINGS",
      "/D",
      "_HAS_ITERATOR_DEBUGGING=0",
      "/wd4018",
      "/wd4100",
      "/wd4127",
      "/wd4244",
      "/wd4267",
      "/wd4701",
      "/wd4706",
      "bzip2.c",
    ],
    tmpDir,
  );
}

async function main() {
  const args = parseArgs();
  await checkout(args.repo, args.rev, args.keep);

  const { header, source } = generateAmalgamation(checkoutDir);
  const version = versionText(args.repo, args.rev);
  await validateCompile(header, source);

  mkdirSync(outDir, { recursive: true });
  const outHeader = join(outDir, "bzlib.h");
  const outSource = join(outDir, "bzip2.c");
  const outVersion = join(outDir, "version.txt");
  rmSync(outHeader, { force: true });
  rmSync(outSource, { force: true });
  rmSync(outVersion, { force: true });
  writeFileSync(outHeader, header);
  writeFileSync(outSource, source);
  writeFileSync(outVersion, version);
  console.log(`wrote ${outHeader}`);
  console.log(`wrote ${outSource}`);
  console.log(`wrote ${outVersion}`);
}

await main();
