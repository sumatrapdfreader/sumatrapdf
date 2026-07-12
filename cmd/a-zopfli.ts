import {
  existsSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, join, normalize, relative } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

type Args = {
  repo: string;
  rev: string;
  keep: boolean;
};

const defaultRepo = "https://github.com/google/zopfli";
const defaultRev = "ccf9f0588d4a4509cb1040310ec122243e670ee6";
const homepage = "https://github.com/google/zopfli";

const depsDir = "deps";
const checkoutDir = join(depsDir, "zopfli");
const outDir = join("ext", "a-zopfli");
const tmpDir = join("cmd", "tmp", "a-zopfli");

const sourcePreamble = `#include "zopflipng/zopflipng_lib.h"
#include "zopflipng/lodepng/lodepng.h"
`;

const zopfliSources = [
  "blocksplitter.c",
  "cache.c",
  "deflate.c",
  "gzip_container.c",
  "hash.c",
  "katajainen.c",
  "lz77.c",
  "squeeze.c",
  "tree.c",
  "util.c",
  "zlib_container.c",
  "zopfli_lib.c",
];

const cppSources = [
  join("zopflipng", "lodepng", "lodepng.cpp"),
  join("zopflipng", "lodepng", "lodepng_util.cpp"),
  join("zopflipng", "zopflipng_lib.cc"),
];

function usage(): never {
  console.error(`Usage: bun cmd/a-zopfli.ts [zopfli-repo-url] [git-tag-or-checkin] [--keep]

Clones/checks out zopfli under deps/zopfli, generates ext/a-zopfli headers and
ext/a-zopfli/zopfli.cpp, validates the amalgamated C++ file with cl.exe, and
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

function normPath(path: string): string {
  return normalize(path).replace(/\\/g, "/");
}

function collectFiles(dir: string): string[] {
  const proc = Bun.spawnSync(["git", "-C", checkoutDir, "ls-files", dir], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (proc.exitCode !== 0) {
    throw new Error(proc.stderr.toString().trim());
  }
  return proc.stdout
    .toString()
    .trim()
    .split(/\r?\n/)
    .filter(Boolean)
    .map((path) => join(checkoutDir, path));
}

function buildIncludeMap(root: string): Map<string, string> {
  const map = new Map<string, string>();
  for (const path of collectFiles("src")) {
    if (!/\.(h|c|cc|cpp)$/.test(path)) {
      continue;
    }
    const rel = normPath(relative(join(root, "src"), path));
    map.set(rel, path);
    map.set(basename(path), path);
  }
  return map;
}

function resolveInclude(
  fromPath: string,
  inc: string,
  byName: Map<string, string>,
): string | undefined {
  const relativePath = normPath(join(dirname(fromPath), inc));
  if (existsSync(relativePath)) {
    return relativePath;
  }
  const srcRel = normPath(relative(join(checkoutDir, "src"), relativePath));
  return byName.get(srcRel) ?? byName.get(inc) ?? byName.get(basename(inc));
}

function expandLocalIncludes(
  path: string,
  byName: Map<string, string>,
  skipIncludes = new Set<string>(),
  includedIncludes?: Set<string>,
  stack: string[] = [],
): string {
  const text = readText(path);
  const lines = text.split("\n");
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
    const incPath = resolveInclude(path, inc, byName);
    if (incPath) {
      if (includedIncludes?.has(incPath)) {
        continue;
      }
      if (stack.includes(incPath)) {
        throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
      }
      includedIncludes?.add(incPath);
      out.push(
        expandLocalIncludes(incPath, byName, skipIncludes, includedIncludes, [
          ...stack,
          path,
        ]),
      );
    } else {
      out.push(line);
    }
  }
  return out.join("\n");
}

function prepareHeader(path: string, byName: Map<string, string>): string {
  return normalizeBlankLines(
    stripComments(
      expandLocalIncludes(path, byName, new Set([basename(path)])),
    ),
  );
}

function prepareChunk(
  path: string,
  byName: Map<string, string>,
  skipIncludes: Set<string>,
  includedIncludes: Set<string>,
): string {
  return normalizeBlankLines(
    stripComments(
      expandLocalIncludes(path, byName, skipIncludes, includedIncludes),
    ),
  );
}

function generateAmalgamation(root: string): {
  lodepngHeader: string;
  zopflipngHeader: string;
  source: string;
} {
  const srcDir = join(root, "src");
  const byName = buildIncludeMap(root);
  const lodepngHeader = prepareHeader(
    join(srcDir, "zopflipng", "lodepng", "lodepng.h"),
    byName,
  );
  const zopflipngHeader = prepareHeader(
    join(srcDir, "zopflipng", "zopflipng_lib.h"),
    byName,
  );

  const skipIncludes = new Set([
    "zopflipng_lib.h",
    "lodepng.h",
    "zopflipng/lodepng/lodepng.h",
    "lodepng/lodepng.h",
  ]);
  const includedIncludes = new Set<string>();
  const chunks = [sourcePreamble];
  for (const name of zopfliSources) {
    chunks.push(
      prepareChunk(
        join(srcDir, "zopfli", name),
        byName,
        skipIncludes,
        includedIncludes,
      ),
    );
  }
  for (const name of cppSources) {
    chunks.push(
      prepareChunk(join(srcDir, name), byName, skipIncludes, includedIncludes),
    );
  }

  return {
    lodepngHeader,
    zopflipngHeader,
    source: normalizeBlankLines(chunks.join("\n")),
  };
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

async function validateCompile(
  lodepngHeader: string,
  zopflipngHeader: string,
  source: string,
): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(join(tmpDir, "zopflipng", "lodepng"), { recursive: true });
  writeFileSync(join(tmpDir, "zopflipng", "lodepng", "lodepng.h"), lodepngHeader);
  writeFileSync(join(tmpDir, "zopflipng", "zopflipng_lib.h"), zopflipngHeader);
  writeFileSync(join(tmpDir, "zopfli.cpp"), source);

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
      "/EHsc",
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
      "/wd4018",
      "/wd4100",
      "/wd4127",
      "/wd4244",
      "/wd4267",
      "/wd4334",
      "/wd4305",
      "/wd4457",
      "/wd4459",
      "/wd4477",
      "/wd4530",
      "/wd4702",
      "/wd4996",
      "zopfli.cpp",
    ],
    tmpDir,
  );
}

async function main() {
  const args = parseArgs();
  await checkout(args.repo, args.rev, args.keep);

  const { lodepngHeader, zopflipngHeader, source } =
    generateAmalgamation(checkoutDir);
  const version = versionText(args.repo, args.rev);
  await validateCompile(lodepngHeader, zopflipngHeader, source);

  mkdirSync(join(outDir, "zopflipng", "lodepng"), { recursive: true });
  const outLodepngHeader = join(outDir, "zopflipng", "lodepng", "lodepng.h");
  const outZopflipngHeader = join(outDir, "zopflipng", "zopflipng_lib.h");
  const outSource = join(outDir, "zopfli.cpp");
  const outVersion = join(outDir, "version.txt");
  rmSync(outLodepngHeader, { force: true });
  rmSync(outZopflipngHeader, { force: true });
  rmSync(outSource, { force: true });
  rmSync(outVersion, { force: true });
  writeFileSync(outLodepngHeader, lodepngHeader);
  writeFileSync(outZopflipngHeader, zopflipngHeader);
  writeFileSync(outSource, source);
  writeFileSync(outVersion, version);
  console.log(`wrote ${outLodepngHeader}`);
  console.log(`wrote ${outZopflipngHeader}`);
  console.log(`wrote ${outSource}`);
  console.log(`wrote ${outVersion}`);
}

await main();
