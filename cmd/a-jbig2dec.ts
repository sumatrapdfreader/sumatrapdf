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

const defaultRepo = "https://github.com/ArtifexSoftware/jbig2dec";
const defaultRev = "dc15c39bbbddc90f79c14563d2eb5a794106be8f";
const homepage = "https://github.com/ArtifexSoftware/jbig2dec";

const depsDir = "deps";
const checkoutDir = join(depsDir, "jbig2dec");
const outDir = join("ext", "a-jbig2dec");
const tmpDir = join("cmd", "tmp", "a-jbig2dec");

const sourceFiles = [
  "jbig2.c",
  "jbig2_arith.c",
  "jbig2_arith_iaid.c",
  "jbig2_arith_int.c",
  "jbig2_generic.c",
  "jbig2_huffman.c",
  "jbig2_hufftab.c",
  "jbig2_halftone.c",
  "jbig2_image.c",
  "jbig2_mmr.c",
  "jbig2_page.c",
  "jbig2_refinement.c",
  "jbig2_segment.c",
  "jbig2_symbol_dict.c",
  "jbig2_text.c",
];

function usage(): never {
  console.error(`Usage: bun cmd/a-jbig2dec.ts [jbig2dec-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out jbig2dec under deps/jbig2dec, generates ext/a-jbig2dec/jbig2.h
and ext/a-jbig2dec/jbig2dec.c, validates the amalgamated C file with cl.exe,
and leaves the generated files ready for the MuPDF build.

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
      .replace(/[ \t]+$/gm, "")
      .replace(/\n{3,}/g, "\n\n")
      .trim() + "\n"
  );
}

function normPath(path: string): string {
  return normalize(path).replace(/\\/g, "/");
}

function resolveInclude(path: string, inc: string): string | undefined {
  const rel = normPath(join(dirname(path), inc));
  if (existsSync(rel)) {
    return rel;
  }
  const rootRel = normPath(join(checkoutDir, inc));
  if (existsSync(rootRel)) {
    return rootRel;
  }
  return undefined;
}

function expandLocalIncludes(
  path: string,
  skipIncludes = new Set<string>(),
  includedIncludes?: Set<string>,
  stack: string[] = [],
): string {
  const out: string[] = [];
  for (const line of readText(path).split("\n")) {
    const m = /^\s*#\s*include\s+"([^"]+)"/.exec(line);
    if (!m) {
      out.push(line);
      continue;
    }

    const inc = m[1];
    if (skipIncludes.has(inc) || skipIncludes.has(basename(inc))) {
      continue;
    }
    const incPath = resolveInclude(path, inc);
    if (!incPath) {
      out.push(line);
      continue;
    }
    if (includedIncludes?.has(incPath)) {
      continue;
    }
    if (stack.includes(incPath)) {
      throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
    }
    includedIncludes?.add(incPath);
    out.push(
      expandLocalIncludes(incPath, skipIncludes, includedIncludes, [
        ...stack,
        path,
      ]),
    );
  }
  return out.join("\n");
}

function prepareHeader(path: string): string {
  return normalizeBlankLines(stripComments(expandLocalIncludes(path)));
}

function prepareChunk(
  path: string,
  skipIncludes: Set<string>,
  includedIncludes: Set<string>,
): string {
  return normalizeBlankLines(
    stripComments(expandLocalIncludes(path, skipIncludes, includedIncludes)),
  );
}

function generateAmalgamation(root: string): { header: string; source: string } {
  const header = normalizeBlankLines(`#include <stddef.h>
#include <stdint.h>

${prepareHeader(join(root, "jbig2.h"))}`);
  const skip = new Set(["config.h", "jbig2.h"]);
  const includedIncludes = new Set<string>();
  const chunks = ['#include "jbig2.h"\n'];
  for (const name of sourceFiles) {
    chunks.push(prepareChunk(join(root, name), skip, includedIncludes));
  }
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
      `github_url: ${homepage}`,
      `github_commit_url: ${homepage}/commit/${commitSha1}`,
    ].join("\n") + "\n"
  );
}

async function validateCompile(header: string, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFileSync(join(tmpDir, "jbig2.h"), header);
  writeFileSync(join(tmpDir, "jbig2dec.c"), source);

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
      "_CRT_SECURE_NO_WARNINGS",
      "/D",
      "HAVE_STRING_H=1",
      "/D",
      "JBIG_NO_MEMENTO",
      "/D",
      "_HAS_ITERATOR_DEBUGGING=0",
      "/wd4018",
      "/wd4100",
      "/wd4146",
      "/wd4244",
      "/wd4267",
      "/wd4456",
      "/wd4701",
      "jbig2dec.c",
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
  writeFileSync(join(outDir, "jbig2.h"), header);
  writeFileSync(join(outDir, "jbig2dec.c"), source);
  writeFileSync(join(outDir, "version.txt"), version);
  console.log(`wrote ${join(outDir, "jbig2.h")}`);
  console.log(`wrote ${join(outDir, "jbig2dec.c")}`);
  console.log(`wrote ${join(outDir, "version.txt")}`);
}

await main();
