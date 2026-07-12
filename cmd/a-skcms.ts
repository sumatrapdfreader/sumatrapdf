import {
  existsSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, join, normalize } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

type Args = { repo: string; rev: string; keep: boolean };

const defaultRepo = "https://skia.googlesource.com/skcms";
const defaultRev = "b2e692629c1fb19342517d7fb61f1cf83d075492";
const homepage = "https://skia.googlesource.com/skcms";

const depsDir = "deps";
const checkoutDir = join(depsDir, "skcms");
const outDir = join("ext", "a-skcms");
const tmpDir = join("cmd", "tmp", "a-skcms");

function usage(): never {
  console.error(`Usage: bun cmd/a-skcms.ts [skcms-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out skcms under deps/skcms, generates ext/a-skcms/skcms.h and
ext/a-skcms/skcms.cpp, validates the amalgamated C++ file with cl.exe, and
leaves the generated files ready for the SumatraPDF build.

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

async function checkout(repo: string, rev: string, keep: boolean): Promise<void> {
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
      .replace(/^\s*#\s*pragma\s+once\s*$/gm, "")
      .replace(/[ \t]+$/gm, "")
      .replace(/\n{3,}/g, "\n\n")
      .trim() + "\n"
  );
}

function normPath(path: string): string {
  return normalize(path).replace(/\\/g, "/");
}

function expandLocalIncludes(
  path: string,
  skipIncludes = new Set<string>(),
  stack: string[] = [],
): string {
  const lines = readText(path).split("\n");
  const out: string[] = [];
  for (const line of lines) {
    const m = /^\s*#\s*include\s+"([^"]+)"/.exec(line);
    if (!m) {
      out.push(line);
      continue;
    }

    const inc = m[1];
    if (skipIncludes.has(inc) || skipIncludes.has(basename(inc))) {
      continue;
    }
    const incPath = normPath(join(dirname(path), inc));
    if (!existsSync(incPath)) {
      out.push(line);
      continue;
    }
    if (stack.includes(incPath)) {
      throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
    }
    out.push(expandLocalIncludes(incPath, skipIncludes, [...stack, path]));
  }
  return out.join("\n");
}

function generateAmalgamation(root: string): { header: string; source: string } {
  const skipIncludes = new Set([
    "src/skcms_public.h",
    "src/skcms_internals.h",
    "src/skcms_Transform.h",
    "skcms_public.h",
    "skcms_internals.h",
    "skcms_Transform.h",
  ]);
  const publicHeader = normalizeBlankLines(
    stripComments(readText(join(root, "src", "skcms_public.h"))),
  );
  const guardedHeader = normalizeBlankLines(`#ifndef SUMATRA_A_SKCMS_H
#define SUMATRA_A_SKCMS_H

${publicHeader}

#endif
`);
  const chunks = [
    '#include "skcms.h"\n',
    normalizeBlankLines(stripComments(readText(join(root, "src", "skcms_internals.h")))),
    normalizeBlankLines(stripComments(readText(join(root, "src", "skcms_Transform.h")))),
    normalizeBlankLines(stripComments(expandLocalIncludes(join(root, "skcms.cc"), skipIncludes))),
    normalizeBlankLines(
      stripComments(
        expandLocalIncludes(
          join(root, "src", "skcms_TransformBaseline.cc"),
          skipIncludes,
        ),
      ),
    ),
  ];
  return { header: guardedHeader, source: normalizeBlankLines(chunks.join("\n")) };
}

function versionText(repo: string, rev: string): string {
  const commitSha1 = gitOutput(["rev-parse", "HEAD"], checkoutDir);
  return (
    [
      `project_homepage: ${homepage}`,
      `repo_url: ${repo}`,
      `revision: ${rev}`,
      `commit_sha1: ${commitSha1}`,
      `source_url: ${homepage}/+/${commitSha1}`,
    ].join("\n") + "\n"
  );
}

async function validateCompile(header: string, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFileSync(join(tmpDir, "skcms.h"), header);
  writeFileSync(join(tmpDir, "skcms.cpp"), source);

  detectVisualStudio2026();
  await runLogged(
    "cl.exe",
    [
      "/nologo",
      "/c",
      "/TP",
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
      "SKCMS_DISABLE_HSW",
      "/D",
      "SKCMS_DISABLE_SKX",
      "/D",
      "_HAS_ITERATOR_DEBUGGING=0",
      "/wd4100",
      "/wd4201",
      "/wd4244",
      "/wd4245",
      "/wd4267",
      "/wd4310",
      "/wd4456",
      "/wd4701",
      "/wd4702",
      "skcms.cpp",
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
  writeFileSync(join(outDir, "skcms.h"), header);
  writeFileSync(join(outDir, "skcms.cpp"), source);
  writeFileSync(join(outDir, "version.txt"), version);
  console.log(`wrote ${join(outDir, "skcms.h")}`);
  console.log(`wrote ${join(outDir, "skcms.cpp")}`);
  console.log(`wrote ${join(outDir, "version.txt")}`);
}

await main();
