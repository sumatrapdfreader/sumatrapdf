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

const defaultRepo = "https://github.com/ArtifexSoftware/extract";
const defaultRev = "8750ac39c30a0d65119b426b5a491c5b8e8bf674";
const homepage = "https://github.com/ArtifexSoftware/extract";

const depsDir = "deps";
const checkoutDir = join(depsDir, "extract");
const outDir = join("ext", "a-extract");
const tmpDir = join("cmd", "tmp", "a-extract");

const sourceFiles = [
  "alloc.c",
  "astring.c",
  "boxer.c",
  "buffer.c",
  "document.c",
  "docx.c",
  "docx_template.c",
  "extract.c",
  "html.c",
  "join.c",
  "json.c",
  "mem.c",
  "memento.c",
  "odt_template.c",
  "odt.c",
  "outf.c",
  "rect.c",
  "sys.c",
  "text.c",
  "xml.c",
  "zip.c",
];

function usage(): never {
  console.error(`Usage: bun cmd/a-extract.ts [extract-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out extract under deps/extract, generates ext/a-extract headers
and ext/a-extract/extract.c, validates the amalgamated C file with cl.exe, and
leaves the generated files ready for the MuPDF build.

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
  for (const dir of [join(checkoutDir, "include"), join(checkoutDir, "src")]) {
    const candidate = normPath(join(dir, inc));
    if (existsSync(candidate)) {
      return candidate;
    }
  }
  return undefined;
}

function shouldKeepInclude(inc: string): boolean {
  return inc.startsWith("extract/") || inc === "memento.h";
}

function expandLocalIncludes(
  path: string,
  includedIncludes: Set<string>,
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
    if (shouldKeepInclude(inc)) {
      out.push(line);
      continue;
    }
    const incPath = resolveInclude(path, inc);
    if (!incPath) {
      out.push(line);
      continue;
    }
    if (includedIncludes.has(incPath)) {
      continue;
    }
    if (stack.includes(incPath)) {
      throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
    }
    includedIncludes.add(incPath);
    out.push(expandLocalIncludes(incPath, includedIncludes, [...stack, path]));
  }
  return out.join("\n");
}

function preparePublicHeader(path: string): string {
  return normalizeBlankLines(stripComments(readText(path)));
}

function prepareChunk(path: string, includedIncludes: Set<string>): string {
  return normalizeBlankLines(
    stripComments(expandLocalIncludes(path, includedIncludes)),
  );
}

function generateAmalgamation(root: string): {
  headers: Map<string, string>;
  source: string;
} {
  const headers = new Map<string, string>();
  for (const name of ["alloc.h", "buffer.h", "extract.h"]) {
    headers.set(join("extract", name), preparePublicHeader(join(root, "include", "extract", name)));
  }
  headers.set("memento.h", preparePublicHeader(join(root, "src", "memento.h")));

  const includedIncludes = new Set<string>();
  const chunks = [
    '#include "extract/extract.h"\n#include "extract/buffer.h"\n#include "memento.h"\n',
  ];
  for (const name of sourceFiles) {
    chunks.push(prepareChunk(join(root, "src", name), includedIncludes));
  }
  return { headers, source: normalizeBlankLines(chunks.join("\n")) };
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

async function validateCompile(headers: Map<string, string>, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(join(tmpDir, "extract"), { recursive: true });
  for (const [name, text] of headers) {
    writeFileSync(join(tmpDir, name), text);
  }
  writeFileSync(join(tmpDir, "extract.c"), source);

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
      "_HAS_ITERATOR_DEBUGGING=0",
      "/I",
      ".",
      "/I",
      "..\\..\\..\\ext\\a-zlib",
      "/wd4005",
      "/wd4100",
      "/wd4127",
      "/wd4130",
      "/wd4201",
      "/wd4245",
      "/wd4310",
      "/wd4389",
      "/wd4456",
      "/wd4457",
      "/wd4701",
      "/wd4996",
      "extract.c",
    ],
    tmpDir,
  );
}

async function main() {
  const args = parseArgs();
  await checkout(args.repo, args.rev, args.keep);

  const { headers, source } = generateAmalgamation(checkoutDir);
  const version = versionText(args.repo, args.rev);
  await validateCompile(headers, source);

  mkdirSync(join(outDir, "extract"), { recursive: true });
  for (const [name, text] of headers) {
    writeFileSync(join(outDir, name), text);
    console.log(`wrote ${join(outDir, name)}`);
  }
  writeFileSync(join(outDir, "extract.c"), source);
  writeFileSync(join(outDir, "version.txt"), version);
  console.log(`wrote ${join(outDir, "extract.c")}`);
  console.log(`wrote ${join(outDir, "version.txt")}`);
}

await main();
