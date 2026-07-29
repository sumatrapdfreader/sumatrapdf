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

const defaultRepo = "https://github.com/ArtifexSoftware/mujs";
const defaultRev = "e892c9fdbbddba94e52f656ccb378ed4885e30cc";
const homepage = "https://mujs.com/";

const depsDir = "deps";
const checkoutDir = join(depsDir, "mujs");
const outDir = join("ext", "a-mujs");
const tmpDir = join("cmd", "tmp", "a-mujs");

const sourcePreamble = `#include "mujs.h"
`;

function usage(): never {
  console.error(`Usage: bun cmd/a-mujs.ts [mujs-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out MuJS under deps/mujs, generates ext/a-mujs/mujs.h and
ext/a-mujs/mujs.c, validates the amalgamated C file with cl.exe, and leaves
the generated files ready for the a-mujs Premake project.

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

function gitOutputMaybe(args: string[], cwd: string): string {
  const proc = Bun.spawnSync(["git", ...args], {
    cwd,
    stdout: "pipe",
    stderr: "pipe",
  });
  if (proc.exitCode !== 0) {
    return "";
  }
  return proc.stdout.toString().trim();
}

function normalizeGithubUrl(repo: string): string | undefined {
  let url = repo.trim();
  const remoteMatch = /^git@github\.com:([^/]+\/[^/]+?)(?:\.git)?$/.exec(url);
  if (remoteMatch) {
    return `https://github.com/${remoteMatch[1]}`;
  }
  url = url.replace(/^ssh:\/\/git@github\.com\//, "https://github.com/");
  if (!url.startsWith("https://github.com/")) {
    return undefined;
  }
  return url.replace(/\.git$/, "").replace(/\/$/, "");
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

function expandLocalIncludes(
  path: string,
  byName: Map<string, string>,
  skipIncludes = new Set<string>(),
  included = new Set<string>(),
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
    if (skipIncludes.has(inc)) {
      continue;
    }
    const incPath = byName.get(inc);
    if (!incPath) {
      out.push(line);
      continue;
    }
    if (included.has(incPath)) {
      continue;
    }
    if (stack.includes(incPath)) {
      throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
    }
    included.add(incPath);
    out.push(
      expandLocalIncludes(incPath, byName, skipIncludes, included, [
        ...stack,
        path,
      ]),
    );
  }
  return out.join("\n");
}

function prepareChunk(
  path: string,
  byName: Map<string, string>,
  skipIncludes = new Set<string>(),
  included?: Set<string>,
): string {
  return normalizeBlankLines(
    stripComments(expandLocalIncludes(path, byName, skipIncludes, included)),
  );
}

function generateAmalgamation(root: string): {
  header: string;
  source: string;
} {
  const byName = new Map(listFiles(root).map((path) => [basename(path), path]));
  const header = prepareChunk(join(root, "mujs.h"), byName);
  const source = normalizeBlankLines(
    [
      sourcePreamble,
      prepareChunk(
        join(root, "one.c"),
        byName,
        new Set(["mujs.h"]),
        new Set<string>(),
      ),
    ].join("\n"),
  );
  return { header, source };
}

function versionText(repo: string, rev: string): string {
  const commitSha1 = gitOutput(["rev-parse", "HEAD"], checkoutDir);
  const originUrl =
    gitOutputMaybe(["config", "--get", "remote.origin.url"], checkoutDir) ||
    repo;
  const githubUrl = normalizeGithubUrl(originUrl);
  const lines = [
    `project_homepage: ${homepage}`,
    `repo_url: ${originUrl}`,
    `revision: ${rev}`,
    `commit_sha1: ${commitSha1}`,
  ];
  if (githubUrl) {
    lines.push(`github_url: ${githubUrl}`);
    lines.push(`github_commit_url: ${githubUrl}/commit/${commitSha1}`);
  }
  return lines.join("\n") + "\n";
}

async function validateCompile(header: string, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFileSync(join(tmpDir, "mujs.h"), header);
  writeFileSync(join(tmpDir, "mujs.c"), source);

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
      "_HAS_ITERATOR_DEBUGGING=0",
      "/wd4090",
      "/wd4100",
      "/wd4127",
      "/wd4146",
      "/wd4310",
      "/wd4324",
      "/wd4702",
      "/wd4706",
      "mujs.c",
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
  writeFileSync(join(outDir, "mujs.h"), header);
  writeFileSync(join(outDir, "mujs.c"), source);
  writeFileSync(join(outDir, "version.txt"), version);
  console.log(`wrote ${join(outDir, "mujs.h")}`);
  console.log(`wrote ${join(outDir, "mujs.c")}`);
  console.log(`wrote ${join(outDir, "version.txt")}`);
}

await main();
