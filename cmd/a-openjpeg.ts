import {
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, join, normalize } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

type Args = { repo: string; rev: string; keep: boolean };

const defaultRepo = "https://github.com/ArtifexSoftware/thirdparty-openjpeg";
const defaultRev = "957029eb875eee1118743f200cb86da9d8289de2";
const homepage = "https://www.openjpeg.org/";

const depsDir = "deps";
const checkoutDir = join(depsDir, "openjpeg");
const outDir = join("ext", "a-openjpeg");
const tmpDir = join("cmd", "tmp", "a-openjpeg");

const sourceFiles = [
  "bio.c",
  "cidx_manager.c",
  "cio.c",
  "dwt.c",
  "event.c",
  "function_list.c",
  "ht_dec.c",
  "image.c",
  "invert.c",
  "j2k.c",
  "jp2.c",
  "mct.c",
  "mqc.c",
  "openjpeg.c",
  "opj_clock.c",
  "phix_manager.c",
  "pi.c",
  "ppix_manager.c",
  "sparse_array.c",
  "t1.c",
  "t2.c",
  "tcd.c",
  "tgt.c",
  "thix_manager.c",
  "thread.c",
  "tpix_manager.c",
];

function usage(): never {
  console.error(`Usage: bun cmd/a-openjpeg.ts [openjpeg-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out OpenJPEG under deps/openjpeg, generates ext/a-openjpeg/openjpeg.h
and ext/a-openjpeg/openjpeg.c, validates the amalgamated C file with cl.exe,
and leaves the generated files ready for the openjpeg build.

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

function sortedFiles(dir: string, ext: string): string[] {
  return readdirSync(dir)
    .map((name) => join(dir, name))
    .filter((path) => statSync(path).isFile() && path.endsWith(ext))
    .sort();
}

function findOpenjp2Dir(root: string): string {
  const candidates = [join(root, "src", "lib", "openjp2"), root];
  for (const dir of candidates) {
    if (existsSync(join(dir, "openjpeg.h")) && existsSync(join(dir, "j2k.c"))) {
      return dir;
    }
  }
  throw new Error(`could not find OpenJPEG openjp2 source directory under ${root}`);
}

function resolveInclude(path: string, inc: string, srcDir: string): string | undefined {
  const rel = normPath(join(dirname(path), inc));
  if (existsSync(rel)) {
    return rel;
  }
  const rootRel = normPath(join(srcDir, inc));
  if (existsSync(rootRel)) {
    return rootRel;
  }
  return undefined;
}

function expandLocalIncludes(
  path: string,
  srcDir: string,
  includedIncludes: Set<string>,
  keepIncludes = new Set<string>(),
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
    if (keepIncludes.has(inc) || keepIncludes.has(basename(inc))) {
      out.push(line);
      continue;
    }
    const incPath = resolveInclude(path, inc, srcDir);
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
    out.push(expandLocalIncludes(incPath, srcDir, includedIncludes, keepIncludes, [...stack, path]));
  }
  return out.join("\n");
}

function prepareHeader(path: string, srcDir: string): string {
  void srcDir;
  return normalizeBlankLines(stripComments(readText(path)));
}

function prepareChunk(path: string, srcDir: string, includedIncludes: Set<string>): string {
  void srcDir;
  void includedIncludes;
  let text = stripComments(readText(path));
  if (basename(path) === "ht_dec.c") {
    text = `#define opj_t1_allocate_buffers opj_ht_dec_t1_allocate_buffers\n${text}\n#undef opj_t1_allocate_buffers\n`;
  }
  return normalizeBlankLines(text);
}

function generateAmalgamation(srcDir: string): { headers: Map<string, string>; source: string } {
  const headers = new Map<string, string>();
  for (const path of sortedFiles(srcDir, ".h")) {
    headers.set(basename(path), prepareHeader(path, srcDir));
  }
  const includedIncludes = new Set<string>();
  const chunks = ['#include "openjpeg.h"\n'];
  for (const name of sourceFiles) {
    chunks.push(prepareChunk(join(srcDir, name), srcDir, includedIncludes));
  }
  return { headers, source: normalizeBlankLines(chunks.join("\n")) };
}

function githubUrl(repo: string): string | undefined {
  const url = repo.replace(/\.git$/, "").replace(/\/$/, "");
  return url.startsWith("https://github.com/") ? url : undefined;
}

function versionText(repo: string, rev: string): string {
  const commitSha1 = gitOutput(["rev-parse", "HEAD"], checkoutDir);
  const github = githubUrl(repo);
  const lines = [
    `project_homepage: ${homepage}`,
    `repo_url: ${repo}`,
    `revision: ${rev}`,
    `commit_sha1: ${commitSha1}`,
  ];
  if (github) {
    lines.push(`github_url: ${github}`);
    lines.push(`github_commit_url: ${github}/commit/${commitSha1}`);
  }
  return lines.join("\n") + "\n";
}

function writeOutput(dir: string, headers: Map<string, string>, source: string, version?: string): void {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const [name, text] of headers) {
    writeFileSync(join(dir, name), text);
  }
  writeFileSync(join(dir, "openjpeg.c"), source);
  if (version) {
    writeFileSync(join(dir, "version.txt"), version);
  }
}

async function validateCompile(headers: Map<string, string>, source: string): Promise<void> {
  writeOutput(tmpDir, headers, source);

  detectVisualStudio2026();
  const clProc = Bun.spawnSync(["where.exe", "cl.exe"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (clProc.exitCode !== 0) {
    throw new Error("could not find cl.exe in PATH");
  }
  await runLogged(
    "cmd.exe",
    [
      "/c",
      [
        "cl.exe",
        "/nologo",
        "/c",
        "/W4",
        "/WX",
        "/O2",
        "/MT",
        "/wd4100",
        "/wd4005",
        "/wd4127",
        "/wd4244",
        "/wd4310",
        "/wd4389",
        "/wd4456",
        "/wd4702",
        "/D_CRT_SECURE_NO_WARNINGS",
        "/DUSE_JPIP",
        "/DOPJ_STATIC",
        "/DOPJ_EXPORTS",
        "/I",
        tmpDir,
        join(tmpDir, "openjpeg.c"),
        "/Fo" + join(tmpDir, "openjpeg.obj"),
      ].join(" "),
    ],
    ".",
  );
}

async function main(): Promise<void> {
  const args = parseArgs();
  await checkout(args.repo, args.rev, args.keep);
  const srcDir = findOpenjp2Dir(checkoutDir);
  const { headers, source } = generateAmalgamation(srcDir);
  await validateCompile(headers, source);
  writeOutput(outDir, headers, source, versionText(args.repo, args.rev));
  console.log(`wrote ${outDir}`);
}

await main();
