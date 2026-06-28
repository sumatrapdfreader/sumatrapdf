// Scan src/ for remaining char*/const char* (and WCHAR*) API surface to port to Str/WStr.
// Usage: bun cmd/scripts/str_port_inventory.ts [output-path]

import { mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";

const repoRoot = join(import.meta.dir, "..", "..");
const srcDir = join(repoRoot, "src");
// Default to gitignored tests/tmp so the audit tool never dirties the repo root.
const defaultOut = join(repoRoot, "tests", "tmp", "str-port-remaining.txt");

const outPath = process.argv[2] ?? defaultOut;

type Kind = "member" | "param" | "return" | "local" | "other";

interface Entry {
  file: string;
  line: number;
  kind: Kind;
  symbol: string;
  text: string;
  exclusion?: string;
}

const charPtrRe = /\b(?:const\s+)?char\s*\*\s*(?:const\s*)?(\w+)?/;
const wcharPtrRe = /\b(?:const\s+)?(?:WCHAR|wchar_t)\s*\*\s*(?:const\s*)?(\w+)?/;
const returnCharRe = /\b(?:const\s+)?char\s*\*\s+(\w+)\s*\(/;
const returnWcharRe = /\b(?:const\s+)?(?:WCHAR|wchar_t)\s*\*\s+(\w+)\s*\(/;
const memberCharRe = /^\s*(?:const\s+)?char\s*\*\s*(?:const\s*)?(\w+)\s*(?:=|;)/;
const memberWcharRe = /^\s*(?:const\s+)?(?:WCHAR|wchar_t)\s*\*\s*(?:const\s*)?(\w+)\s*(?:=|;)/;

function walk(dir: string): string[] {
  const res: string[] = [];
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) {
      res.push(...walk(p));
    } else if (/\.(h|cpp|c)$/i.test(name)) {
      res.push(p);
    }
  }
  return res;
}

function tagExclusion(line: string, kind: Kind, contextLines: string[]): string | undefined {
  const t = line.trim();
  if (t.startsWith("//") || t.startsWith("/*") || t.startsWith("*")) {
    return "comment";
  }
  if (/\bextern\s+"C"/.test(t)) {
    return "extern-c";
  }
  if (/\b(?:printf|fprintf|sprintf|snprintf|sscanf|scanf|str::Parse|logf|logfa|CliPrintf)\s*\(/.test(t)) {
    return "format-string";
  }
  if (/\bMAKEINTRESOURCEW?\s*\(/.test(t)) {
    return "win32-resource";
  }
  if (/\bchar\s*\*\s*p\s*=\s*nullptr/.test(t) && /\*\s*p\s*=/.test(t)) {
    return "crash-test";
  }
  if (kind === "local" && /\b(?:cursor|p|s|src|dst|start|end|pos|it|iter)\b/.test(t)) {
    return "parse-cursor";
  }
  const block = [line, ...contextLines].join("\n");
  if (/\bstr-port:/i.test(block)) {
    return "api-boundary";
  }
  return undefined;
}

// Returns true when a line inside a block comment contains the closing star-slash sequence.
function blockCommentCloses(line: string): boolean {
  const closeIdx = line.indexOf("*/");
  if (closeIdx < 0) {
    return false;
  }
  const afterClose = line.slice(closeIdx + 2);
  if (!afterClose.includes("/*")) {
    return true;
  }
  const openIdx = afterClose.indexOf("/*");
  const afterOpen = afterClose.slice(openIdx + 2);
  return afterOpen.includes("*/");
}

export function scanFileContent(rel: string, content: string): Entry[] {
  const lines = content.split(/\r?\n/);
  const entries: Entry[] = [];

  let inBlockComment = false;
  let if0Depth = 0;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const n = i + 1;

    // Track /* */ block comments (including multi-line license headers).
    if (inBlockComment) {
      if (blockCommentCloses(line)) {
        inBlockComment = false;
      }
      continue;
    }
    if (line.includes("/*")) {
      const openIdx = line.indexOf("/*");
      const after = line.slice(openIdx + 2);
      if (!after.includes("*/")) {
        inBlockComment = true;
      }
      // Single-line /* ... */ — skip if entire hit is inside comment.
      const beforeComment = line.slice(0, openIdx).trim();
      if (!beforeComment) {
        continue;
      }
    }

    // Skip disabled preprocessor blocks (#if 0).
    const trimmed = line.trim();
    if (/^#\s*if\s+0\b/.test(trimmed)) {
      if0Depth++;
      continue;
    }
    if (if0Depth > 0) {
      if (/^#\s*if(?:def|ndef)?\b/.test(trimmed) || /^#\s*if\s+\d/.test(trimmed)) {
        if0Depth++;
      } else if (/^#\s*endif\b/.test(trimmed)) {
        if0Depth--;
      }
      continue;
    }

    const contextLines = lines.slice(i + 1, Math.min(i + 4, lines.length));
    const exclusionCtx = (kind: Kind) => tagExclusion(line, kind, contextLines);

    if (memberCharRe.test(line)) {
      const m = line.match(memberCharRe);
      const sym = m?.[1] ?? "?";
      entries.push({
        file: rel,
        line: n,
        kind: "member",
        symbol: sym,
        text: line.trimEnd(),
        exclusion: exclusionCtx("member"),
      });
      continue;
    }
    if (memberWcharRe.test(line)) {
      const m = line.match(memberWcharRe);
      const sym = m?.[1] ?? "?";
      entries.push({
        file: rel,
        line: n,
        kind: "member",
        symbol: sym,
        text: line.trimEnd(),
        exclusion: exclusionCtx("member"),
      });
      continue;
    }

    if (returnCharRe.test(line)) {
      const m = line.match(returnCharRe);
      entries.push({
        file: rel,
        line: n,
        kind: "return",
        symbol: m?.[1] ?? "?",
        text: line.trimEnd(),
        exclusion: exclusionCtx("return"),
      });
    } else if (returnWcharRe.test(line)) {
      const m = line.match(returnWcharRe);
      entries.push({
        file: rel,
        line: n,
        kind: "return",
        symbol: m?.[1] ?? "?",
        text: line.trimEnd(),
        exclusion: exclusionCtx("return"),
      });
    }

    if (charPtrRe.test(line) || wcharPtrRe.test(line)) {
      // Match first param (open-paren immediately before char*) and later params (comma before char*).
      const ptrSig = "(?:const\\s+)?(?:char|WCHAR|wchar_t)\\s*\\*";
      const isParam =
        new RegExp("\\w\\s*\\(\\s*" + ptrSig).test(line) ||
        new RegExp("[(,]\\s*" + ptrSig).test(line);
      if (isParam) {
        const m = line.match(charPtrRe) ?? line.match(wcharPtrRe);
        entries.push({
          file: rel,
          line: n,
          kind: "param",
          symbol: m?.[1] ?? "?",
          text: line.trimEnd(),
          exclusion: exclusionCtx("param"),
        });
      }
    }
  }

  return entries;
}

export function scanFile(path: string): Entry[] {
  const rel = relative(repoRoot, path).replaceAll("\\", "/");
  const content = readFileSync(path, "utf-8");
  return scanFileContent(rel, content);
}

export function countScannedLines(content: string): number {
  let inBlockComment = false;
  let if0Depth = 0;
  let n = 0;
  for (const line of content.split(/\r?\n/)) {
    if (inBlockComment) {
      if (blockCommentCloses(line)) {
        inBlockComment = false;
      }
      continue;
    }
    if (line.includes("/*")) {
      const openIdx = line.indexOf("/*");
      const after = line.slice(openIdx + 2);
      if (!after.includes("*/")) {
        inBlockComment = true;
      }
      const beforeComment = line.slice(0, openIdx).trim();
      if (!beforeComment) {
        continue;
      }
    }
    const trimmed = line.trim();
    if (/^#\s*if\s+0\b/.test(trimmed)) {
      if0Depth++;
      continue;
    }
    if (if0Depth > 0) {
      if (/^#\s*if(?:def|ndef)?\b/.test(trimmed) || /^#\s*if\s+\d/.test(trimmed)) {
        if0Depth++;
      } else if (/^#\s*endif\b/.test(trimmed)) {
        if0Depth--;
      }
      continue;
    }
    n++;
  }
  return n;
}

function runInventory(out: string) {
  const files = walk(srcDir).sort();
  const all = files.flatMap(scanFile);
  const mustConvert = all.filter((e) => !e.exclusion);

  const lines: string[] = [];
  lines.push("# Str port inventory - " + new Date().toISOString());
  lines.push(`# scanned ${files.length} files under src/`);
  lines.push(`# total hits: ${all.length}, must-convert (untagged): ${mustConvert.length}`);
  lines.push("");

  for (const e of all) {
    const ex = e.exclusion ? ` exclusion=${e.exclusion}` : "";
    lines.push(`${e.file}:${e.line}\t${e.kind}\t${e.symbol}${ex}`);
    lines.push(`  ${e.text}`);
  }

  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, lines.join("\n") + "\n", "utf-8");
  console.log(`Wrote ${all.length} entries (${mustConvert.length} untagged) to ${out}`);
  return { total: all.length, untagged: mustConvert.length, mustConvert };
}

if (import.meta.main) {
  runInventory(outPath);
}