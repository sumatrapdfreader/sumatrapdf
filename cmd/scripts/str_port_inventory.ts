// Scan src/ for remaining char*/const char* (and WCHAR*) API surface to port to Str/WStr.
// Usage: bun cmd/scripts/str_port_inventory.ts [output-path]

import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join, relative } from "node:path";

const repoRoot = join(import.meta.dir, "..", "..");
const srcDir = join(repoRoot, "src");
const defaultOut = join(repoRoot, "str-port-remaining.txt");

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

function tagExclusion(line: string, kind: Kind): string | undefined {
  const t = line.trim();
  if (t.startsWith("//") || t.startsWith("/*") || t.startsWith("*")) {
    return "comment";
  }
  if (/\bextern\s+"C"/.test(t)) {
    return "extern-c";
  }
  if (/\b(?:printf|fprintf|sprintf|snprintf|sscanf|scanf|str::Parse|logf|logfa)\s*\(/.test(t)) {
    return "format-string";
  }
  if (kind === "local" && /\b(?:cursor|p|s|src|dst|start|end|pos|it|iter)\b/.test(t)) {
    return "parse-cursor";
  }
  if (/\bstr-port:\s*(?:mupdf|api-boundary|Win32|owned heap)/i.test(t)) {
    return "api-boundary";
  }
  return undefined;
}

function scanFile(path: string): Entry[] {
  const rel = relative(repoRoot, path).replaceAll("\\", "/");
  const lines = readFileSync(path, "utf-8").split(/\r?\n/);
  const entries: Entry[] = [];

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const n = i + 1;

    if (memberCharRe.test(line)) {
      const m = line.match(memberCharRe);
      const sym = m?.[1] ?? "?";
      entries.push({
        file: rel,
        line: n,
        kind: "member",
        symbol: sym,
        text: line.trimEnd(),
        exclusion: tagExclusion(line, "member"),
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
        exclusion: tagExclusion(line, "member"),
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
        exclusion: tagExclusion(line, "return"),
      });
    } else if (returnWcharRe.test(line)) {
      const m = line.match(returnWcharRe);
      entries.push({
        file: rel,
        line: n,
        kind: "return",
        symbol: m?.[1] ?? "?",
        text: line.trimEnd(),
        exclusion: tagExclusion(line, "return"),
      });
    }

    if (charPtrRe.test(line) || wcharPtrRe.test(line)) {
      const isParam = /\([^)]*(?:const\s+)?(?:char|WCHAR|wchar_t)\s*\*/.test(line);
      if (isParam) {
        const m = line.match(charPtrRe) ?? line.match(wcharPtrRe);
        entries.push({
          file: rel,
          line: n,
          kind: "param",
          symbol: m?.[1] ?? "?",
          text: line.trimEnd(),
          exclusion: tagExclusion(line, "param"),
        });
      }
    }
  }

  return entries;
}

const files = walk(srcDir).sort();
const all = files.flatMap(scanFile);
const mustConvert = all.filter((e) => !e.exclusion);

const lines: string[] = [];
lines.push(`# Str port inventory — ${new Date().toISOString()}`);
lines.push(`# scanned ${files.length} files under src/`);
lines.push(`# total hits: ${all.length}, must-convert (untagged): ${mustConvert.length}`);
lines.push("");

for (const e of all) {
  const ex = e.exclusion ? ` exclusion=${e.exclusion}` : "";
  lines.push(`${e.file}:${e.line}\t${e.kind}\t${e.symbol}${ex}`);
  lines.push(`  ${e.text}`);
}

writeFileSync(outPath, lines.join("\n") + "\n", "utf-8");
console.log(`Wrote ${all.length} entries (${mustConvert.length} untagged) to ${outPath}`);