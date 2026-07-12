import {
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
  cpSync,
} from "node:fs";
import { basename, join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

type Args = {
  repo: string;
  rev: string;
  keep: boolean;
};

const depsDir = "deps";
const checkoutDir = join(depsDir, "gumbo");
const outDir = join("ext", "a-gumbo");
const tmpDir = join("cmd", "tmp", "a-gumbo");

const headerPreamble = "";

const sourcePreamble = `#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "gumbo.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
`;

const sumatraHeaderAdditions = `
void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node);
void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output);
`;

const sumatraSourceAdditions = `
void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node) {
  GumboParser parser;
  parser._options = options;

  GumboVector stack;
  gumbo_vector_init(&parser, 10, &stack);
  gumbo_vector_add(&parser, node, &stack);
  while (stack.length > 0) {
    GumboNode* n = (GumboNode*) gumbo_vector_pop(&parser, &stack);
    switch (n->type) {
      case GUMBO_NODE_DOCUMENT: {
        GumboDocument* doc = &n->v.document;
        for (unsigned int i = 0; i < doc->children.length; ++i) {
          gumbo_vector_add(&parser, doc->children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, (void*) doc->children.data);
        gumbo_parser_deallocate(&parser, (void*) doc->name);
        gumbo_parser_deallocate(&parser, (void*) doc->public_identifier);
        gumbo_parser_deallocate(&parser, (void*) doc->system_identifier);
      } break;
      case GUMBO_NODE_TEMPLATE:
      case GUMBO_NODE_ELEMENT:
        for (unsigned int i = 0; i < n->v.element.attributes.length; ++i) {
          gumbo_destroy_attribute(&parser, n->v.element.attributes.data[i]);
        }
        gumbo_parser_deallocate(&parser, n->v.element.attributes.data);
        for (unsigned int i = 0; i < n->v.element.children.length; ++i) {
          gumbo_vector_add(&parser, n->v.element.children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, n->v.element.children.data);
        break;
      case GUMBO_NODE_TEXT:
      case GUMBO_NODE_CDATA:
      case GUMBO_NODE_COMMENT:
      case GUMBO_NODE_WHITESPACE:
        gumbo_parser_deallocate(&parser, (void*) n->v.text.text);
        break;
    }
    gumbo_parser_deallocate(&parser, n);
  }
  gumbo_vector_destroy(&parser, &stack);
}

void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output) {
  GumboParser parser;
  parser._options = options;
  gumbo_destroy_node_iter((GumboOptions*) options, output->document);
  for (unsigned int i = 0; i < output->errors.length; ++i) {
    gumbo_error_destroy(&parser, output->errors.data[i]);
  }
  gumbo_vector_destroy(&parser, &output->errors);
  gumbo_parser_deallocate(&parser, output);
}
`;

function usage(): never {
  console.error(`Usage: bun cmd/a-gumbo.ts <gumbo-repo-url-or-local-path> <git-tag-or-checkin> [--keep]

Clones/checks out Gumbo under deps/gumbo, generates ext/a-gumbo/gumbo.h and
ext/a-gumbo/gumbo.c, validates the amalgamated C file with cl.exe, and leaves
the generated files ready for the a-gumbo Premake project.

Examples:
  bun cmd/a-gumbo.ts https://github.com/google/gumbo-parser.git v0.10.1
  bun cmd/a-gumbo.ts ext/gumbo-parser HEAD
`);
  process.exit(1);
}

function parseArgs(): Args {
  const args = process.argv.slice(2);
  if (args.length < 2 || args.includes("-h") || args.includes("--help")) {
    usage();
  }
  return {
    repo: args[0],
    rev: args[1],
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
    if (existsSync(repo)) {
      cpSync(repo, checkoutDir, { recursive: true });
      if (!existsSync(join(checkoutDir, ".git"))) {
        return;
      }
    } else {
      await runLogged("git", ["clone", repo, checkoutDir]);
    }
  }

  if (existsSync(join(checkoutDir, ".git"))) {
    await runLogged("git", ["-C", checkoutDir, "fetch", "--tags", "--force"]);
    await runLogged("git", ["-C", checkoutDir, "checkout", "--force", rev]);
  }
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

function versionText(repo: string, rev: string): string {
  let repoUrl = repo;
  let commitSha1 = rev;
  if (existsSync(join(checkoutDir, ".git"))) {
    commitSha1 = gitOutput(["rev-parse", "HEAD"], checkoutDir);
    const originUrl = gitOutputMaybe(
      ["config", "--get", "remote.origin.url"],
      checkoutDir,
    );
    if (originUrl) {
      repoUrl = originUrl;
    }
  }

  const githubUrl = normalizeGithubUrl(repoUrl);
  const lines = [
    `repo_url: ${repoUrl}`,
    `revision: ${rev}`,
    `commit_sha1: ${commitSha1}`,
  ];
  if (githubUrl) {
    lines.push(`github_url: ${githubUrl}`);
    lines.push(`github_commit_url: ${githubUrl}/commit/${commitSha1}`);
  }
  return lines.join("\n") + "\n";
}

function listFiles(dir: string, suffix: string): string[] {
  return readdirSync(dir)
    .filter((name) => name.endsWith(suffix))
    .map((name) => join(dir, name))
    .filter((path) => statSync(path).isFile())
    .sort((a, b) => basename(a).localeCompare(basename(b)));
}

function internalIncludes(text: string): string[] {
  const res: string[] = [];
  const re = /^\s*#\s*include\s+"([^"]+)"/gm;
  for (;;) {
    const m = re.exec(text);
    if (!m) {
      break;
    }
    res.push(m[1]);
  }
  return res;
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

function removeObsoleteSystemIncludes(text: string): string {
  return text
    .split("\n")
    .filter((line) => !/^\s*#\s*include\s+<strings\.h>/.test(line))
    .filter((line) => !/^\s*#\s*define\s+_CRT_SECURE_NO_WARNINGS/.test(line))
    .join("\n");
}

function expandLocalIncludes(
  path: string,
  byName: Map<string, string>,
  skipIncludes = new Set<string>(),
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
    if (skipIncludes.has(inc)) {
      continue;
    }
    const incPath = byName.get(inc);
    if (!incPath) {
      out.push(line);
      continue;
    }
    if (stack.includes(incPath)) {
      throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
    }
    out.push(`/* begin ${inc} */`);
    out.push(
      expandLocalIncludes(incPath, byName, skipIncludes, [...stack, path]),
    );
    out.push(`/* end ${inc} */`);
  }
  return out.join("\n");
}

function prepareExpandedChunk(
  path: string,
  byName: Map<string, string>,
  skipIncludes = new Set<string>(),
): string {
  return normalizeBlankLines(
    removeObsoleteSystemIncludes(
      stripComments(expandLocalIncludes(path, byName, skipIncludes)),
    ),
  );
}

function findGumboSrcDir(root: string): string {
  const candidates = [join(root, "src"), root];
  for (const dir of candidates) {
    if (existsSync(join(dir, "gumbo.h"))) {
      return dir;
    }
  }
  throw new Error(`could not find src/gumbo.h under ${root}`);
}

function generateAmalgamation(root: string): {
  header: string;
  source: string;
} {
  const srcDir = findGumboSrcDir(root);
  const gumboHeader = join(srcDir, "gumbo.h");
  const headers = listFiles(srcDir, ".h");
  const byName = new Map(headers.map((path) => [basename(path), path]));
  const sources = listFiles(srcDir, ".c");

  let header = headerPreamble + prepareExpandedChunk(gumboHeader, byName);
  const lastEndif = header.lastIndexOf("#endif");
  if (lastEndif < 0) {
    throw new Error("could not find final #endif in gumbo.h");
  }
  header =
    header.slice(0, lastEndif) +
    normalizeBlankLines(sumatraHeaderAdditions) +
    header.slice(lastEndif);
  const chunks: string[] = [sourcePreamble];
  const sourceSkipIncludes = new Set(["gumbo.h"]);
  for (const c of sources) {
    chunks.push(prepareExpandedChunk(c, byName, sourceSkipIncludes));
  }
  chunks.push(normalizeBlankLines(sumatraSourceAdditions));
  return { header, source: normalizeBlankLines(chunks.join("\n")) };
}

async function validateCompile(header: string, source: string): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFileSync(join(tmpDir, "gumbo.h"), header);
  writeFileSync(join(tmpDir, "gumbo.c"), source);

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
      "/wd4018",
      "/wd4100",
      "/wd4132",
      "/wd4189",
      "/wd4204",
      "/wd4244",
      "/wd4245",
      "/wd4267",
      "/wd4305",
      "/wd4306",
      "/wd4389",
      "/wd4456",
      "/wd4701",
      "/wd4702",
      "gumbo.c",
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
  const outHeader = join(outDir, "gumbo.h");
  const outSource = join(outDir, "gumbo.c");
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
