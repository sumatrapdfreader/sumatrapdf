// Issue #5875: Documentation Ctrl+K search returned many false positives.
// Root cause: parseAllDocs used markers[i+1].index but only stored `start`, so
// every document's content extended to the end of all-docs.md.

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone } from "./util.ts";

function extractFn(src: string, name: string): string {
  const re = new RegExp(`function ${name}\\([^{]*\\)\\s*\\{`);
  const m = re.exec(src);
  if (!m) {
    throw new Error(`issue-5875: ${name} not found in gen_docs.fulltext_search.js`);
  }
  let i = m.index + m[0].length;
  let depth = 1;
  while (i < src.length && depth > 0) {
    const c = src[i++];
    if (c === "{") depth++;
    else if (c === "}") depth--;
  }
  return src.slice(m.index, i);
}

type DocFile = { file: string; title: string; lines: string[] };
type SearchHit = { file: string; title: string; text: string };

function loadSearchFns(): {
  parseAllDocs: (text: string) => DocFile[];
  searchAllDocs: (files: DocFile[], query: string) => SearchHit[];
} {
  const src = readFileSync(join(ROOT, "docs", "gen_docs.fulltext_search.js"), "utf-8");
  // Guard: the bug was using markers[i+1].index without storing index.
  if (!/index:\s*match\.index/.test(src)) {
    throw new Error("issue-5875: parseAllDocs must store match.index (doc end bound); fix regressed");
  }
  const parseSrc = extractFn(src, "parseAllDocs");
  const lineMatchesSrc = extractFn(src, "lineMatchesAll");
  const searchSrc = extractFn(src, "searchAllDocs");
  // searchAllDocs closes over kMaxResults and lineMatchesAll
  const factory = new Function(
    `${lineMatchesSrc}\nconst kMaxResults = 32;\n${parseSrc}\n${searchSrc}\nreturn { parseAllDocs, searchAllDocs };`,
  );
  return factory();
}

export async function testit(): Promise<void> {
  const { parseAllDocs, searchAllDocs } = loadSearchFns();

  // Synthetic: only Commands.md and Late.md mention both "show" and "links".
  // With the old bug, Early.md's content included later files and matched too.
  const corpus = [
    "::Early.md",
    "# Early Doc",
    "unique early content only",
    "",
    "::Commands.md",
    "# Commands",
    'CmdToggleLinks,,Toggle Show Links,"Toggle drawing blue rectangle around links"',
    "",
    "::Late.md",
    "# Late Doc",
    "See show links in the settings.",
    "",
  ].join("\n");

  const files = parseAllDocs(corpus);
  if (files.length !== 3) {
    throw new Error(`expected 3 docs, got ${files.length}`);
  }
  const early = files.find((f) => f.file === "Early.md");
  if (!early) {
    throw new Error("Early.md missing");
  }
  const earlyText = early.lines.join("\n");
  if (earlyText.includes("Show Links") || earlyText.includes("show links")) {
    throw new Error("Early.md content must not include later docs (parseAllDocs end-bound bug)");
  }
  if (!earlyText.includes("unique early content only")) {
    throw new Error("Early.md missing its own content");
  }

  const hits = searchAllDocs(files, "show links");
  const hitFiles = hits.map((h) => h.file);
  if (hitFiles.includes("Early.md")) {
    throw new Error(`false positive Early.md in results: ${hitFiles.join(", ")}`);
  }
  if (!hitFiles.includes("Commands.md")) {
    throw new Error(`expected Commands.md in results, got: ${hitFiles.join(", ")}`);
  }
  if (!hitFiles.includes("Late.md")) {
    throw new Error(`expected Late.md in results, got: ${hitFiles.join(", ")}`);
  }

  // Against real all-docs.md when present (after gen-docs): "show links" must
  // not flood with ~32 junk hits from early docs absorbing the whole corpus.
  const allDocsPath = join(ROOT, ".work", "docs", "all-docs.md");
  if (existsSync(allDocsPath)) {
    const real = parseAllDocs(readFileSync(allDocsPath, "utf-8"));
    const realHits = searchAllDocs(real, "show links");
    if (realHits.length === 0) {
      throw new Error('real all-docs.md: expected some hits for "show links"');
    }
    if (realHits.length >= 32) {
      throw new Error(`real all-docs.md: "show links" returned ${realHits.length} hits (likely parse bug)`);
    }
    const names = realHits.map((h) => h.title || h.file).join("; ");
    // Commands page should be among real hits (CmdToggleLinks / Show Links)
    const hasCommands = realHits.some(
      (h) => /commands/i.test(h.file) || /commands/i.test(h.title) || /show links/i.test(h.text),
    );
    if (!hasCommands) {
      throw new Error(`real all-docs.md: no Commands/show-links hit among: ${names}`);
    }
  }
}

if (import.meta.main) {
  // pure JS unit test; no app build needed
  if (!process.argv.includes("--no-build")) {
    process.argv.push("--no-build");
  }
  await runStandalone(testit);
}
