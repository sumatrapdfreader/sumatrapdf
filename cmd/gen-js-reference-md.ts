import { readFileSync, writeFileSync, renameSync, unlinkSync } from "node:fs";
import { join } from "node:path";

const refDir = join("mupdf", "docs", "reference", "javascript");
const outPath = join("docs", "md", "Tool-run-javascript-reference.md");

const commonTypes = [
  "Archive",
  "Buffer",
  "Color",
  "ColorSpace",
  "DOM",
  "DefaultColorSpaces",
  "Device",
  "DisplayList",
  "DisplayListDevice",
  "Document",
  "DocumentWriter",
  "DrawDevice",
  "Font",
  "Image",
  "Link",
  "LinkDestination",
  "Matrix",
  "MultiArchive",
  "OutlineItem",
  "OutlineIterator",
  "Page",
  "Path",
  "PathWalker",
  "Pixmap",
  "Point",
  "Quad",
  "Rect",
  "Shade",
  "Story",
  "StrokeState",
  "StructuredText",
  "StructuredTextWalker",
  "Text",
  "TextWalker",
  "TreeArchive",
];

const pdfTypes = [
  "PDFAnnotation",
  "PDFDocument",
  "PDFFilespecParams",
  "PDFGraftMap",
  "PDFObject",
  "PDFPage",
  "PDFProcessor",
  "PDFWidget",
];

function slugify(text: string): string {
  return text
    .toLowerCase()
    .replace(/[^\w -]/g, "")
    .replace(/ /g, "-");
}

function convertInline(s: string): string {
  return s
    .replace(/:func:`([^`]+)`/g, "`$1`")
    .replace(/:class:`([^`]+)`/g, "`$1`")
    .replace(/:meth:`([^`]+)`/g, "`$1`")
    .replace(/:attr:`([^`]+)`/g, "`$1`")
    .replace(/:data:`([^`]+)`/g, "`$1`")
    .replace(/:ref:`([^`]+)`/g, "`$1`")
    .replace(/``([^`]+)``/g, "`$1`")
    .replace(/`([^`]+)`_/g, "`$1`")
    .replace(/`([^<`]+) <([^>]+)>`_/g, "[$1]($2)")
    .replace(/\|only_mutool\|/g, "**mutool run only**")
    .replace(/\|only_mupdfjs\|/g, "**mupdf.js only**")
    .replace(/\|no_new\|/g, "*(not constructible with `new`)*")
    .replace(/\|interface_type\|/g, "*(interface type)*");
}

function formatParam(trimmed: string): string | null {
  const m = trimmed.match(/^:param\s+(.+)$/);
  if (!m) {
    return null;
  }
  const rest = m[1];
  const colonIdx = rest.lastIndexOf(":");
  if (colonIdx < 0) {
    return null;
  }
  const typeAndName = rest.slice(0, colonIdx).trim();
  const desc = rest.slice(colonIdx + 1).trim();
  return `- **${typeAndName}**: ${convertInline(desc)}`;
}

function formatField(line: string): string {
  const trimmed = line.trim();
  const param = formatParam(trimmed);
  if (param) {
    return param;
  }
  let m = trimmed.match(/^:param\s+(.+):\s*(.*)$/);
  if (m) {
    return `- **${m[1].trim()}**: ${convertInline(m[2].trim())}`;
  }
  m = trimmed.match(/^:returns:\s*(.*)$/);
  if (m) {
    return `- **Returns:** ${convertInline(m[1].trim())}`;
  }
  m = trimmed.match(/^:throws:\s*(.*)$/);
  if (m) {
    return `- **Throws:** ${convertInline(m[1].trim())}`;
  }
  m = trimmed.match(/^:rtype:\s*(.*)$/);
  if (m) {
    return `- **Type:** ${convertInline(m[1].trim())}`;
  }
  return convertInline(trimmed);
}

function isHeadingUnderline(line: string): boolean {
  return /^[=+\-#]{2,}$/.test(line);
}

function isSkippableTopDirective(line: string): boolean {
  return /^\.\. (default-domain|highlight|toctree)::/.test(line);
}

function isSkippableDirective(line: string): boolean {
  return (
    /^\.\. (default-domain|highlight|toctree|imagesvg|image)::/.test(line) ||
    /^\.\. _[^:]+:\s*$/.test(line) ||
    /^\.\. TODO\b/.test(line)
  );
}

function skipDirectiveOptions(lines: string[], start: number): number {
  let i = start;
  while (i < lines.length && lines[i].match(/^\s*:/)) {
    i++;
  }
  return i;
}

function parseListTable(
  lines: string[],
  start: number,
): { md: string[]; next: number } {
  let i = skipDirectiveOptions(lines, start + 1);
  const rows: string[][] = [];
  while (i < lines.length && !lines[i].trim()) {
    i++;
  }
  while (i < lines.length) {
    const line = lines[i];
    if (!line.match(/^\s*\*\s+-/)) {
      break;
    }
    const cells: string[] = [];
    cells.push(line.replace(/^\s*\*\s+-/, "").trim());
    i++;
    while (i < lines.length && lines[i].match(/^\s+-\s/)) {
      cells.push(lines[i].replace(/^\s+-\s/, "").trim());
      i++;
    }
    rows.push(cells.map(convertInline));
  }
  return { md: markdownTableFromRows(rows), next: i };
}

function parseGridTable(
  lines: string[],
  start: number,
): { md: string[]; next: number } {
  let i = skipDirectiveOptions(lines, start + 1);
  const rows: string[][] = [];
  while (i < lines.length) {
    const line = lines[i];
    if (!line.trim()) {
      i++;
      continue;
    }
    if (/^\s*\.\. \w+::/.test(line)) {
      break;
    }
    if (/^\s*:(param|returns|throws|rtype)\b/.test(line)) {
      break;
    }
    if (/^\s*[=+\-]{2,}[\t\s=\-+]*$/.test(line.trim())) {
      i++;
      continue;
    }
    if (rows.length > 0 && !line.includes("\t")) {
      break;
    }
    const cells = line
      .trim()
      .split(/\t+/)
      .map((c) => convertInline(c.trim()))
      .filter((c) => c.length > 0);
    if (cells.length > 0) {
      rows.push(cells);
    }
    i++;
  }
  return { md: markdownTableFromRows(rows), next: i };
}

function markdownTableFromRows(rows: string[][]): string[] {
  if (rows.length === 0) {
    return [];
  }
  const width = Math.max(...rows.map((r) => r.length));
  const norm = rows.map((r) => {
    const copy = [...r];
    while (copy.length < width) {
      copy.push("");
    }
    return copy;
  });
  const md: string[] = [];
  md.push("| " + norm[0].join(" | ") + " |");
  md.push("| " + norm[0].map(() => "---").join(" | ") + " |");
  for (let r = 1; r < norm.length; r++) {
    md.push("| " + norm[r].join(" | ") + " |");
  }
  md.push("");
  return md;
}

function renderCodeBlock(
  lines: string[],
  start: number,
): { md: string[]; next: number } {
  const langMatch = lines[start].match(/^\.\. code-block::\s*(\w*)/);
  const lang = langMatch?.[1] ?? "";
  const fenceLang = lang === "javascript" ? "js" : lang === "default" ? "" : lang;
  let i = start + 1;
  while (i < lines.length && !lines[i].trim()) {
    i++;
  }
  const codeLines: string[] = [];
  while (i < lines.length) {
    const line = lines[i];
    if (!line.trim()) {
      break;
    }
    if (line.match(/^\.\. \w+::/)) {
      break;
    }
    codeLines.push(line);
    i++;
  }
  if (codeLines.length === 0) {
    return { md: [], next: i };
  }
  return {
    md: ["```" + fenceLang, ...codeLines, "```", ""],
    next: i,
  };
}

function collectIndentedBlock(
  lines: string[],
  start: number,
): { block: string[]; next: number } {
  const block: string[] = [];
  let i = start;
  while (i < lines.length && !lines[i].trim()) {
    i++;
  }
  while (i < lines.length) {
    const line = lines[i];
    if (line.startsWith("\t")) {
      block.push(line.replace(/^\t+/, ""));
      i++;
      continue;
    }
    if (!line.trim()) {
      let j = i + 1;
      while (j < lines.length && !lines[j].trim()) {
        j++;
      }
      if (j < lines.length && lines[j].startsWith("\t")) {
        block.push("");
        i++;
        continue;
      }
      break;
    }
    break;
  }
  return { block, next: i };
}

function normalizeIndent(line: string): string {
  return line.replace(/^\t+/, "");
}

function processBody(lines: string[]): string[] {
  const out: string[] = [];
  let i = 0;
  while (i < lines.length) {
    const line = normalizeIndent(lines[i]);
    if (!line.trim()) {
      out.push("");
      i++;
      continue;
    }
    if (line.match(/^\.\. code-block::/)) {
      const { md, next } = renderCodeBlock(
        lines.map(normalizeIndent),
        i,
      );
      out.push(...md);
      i = next;
      continue;
    }
    if (line.match(/^\.\. list-table::/)) {
      const { md, next } = parseListTable(lines.map(normalizeIndent), i);
      out.push(...md);
      i = next;
      continue;
    }
    if (line.match(/^\.\. table::/)) {
      const { md, next } = parseGridTable(lines.map(normalizeIndent), i);
      out.push(...md);
      i = next;
      continue;
    }
    if (isSkippableDirective(line)) {
      i = skipDirectiveOptions(lines, i + 1);
      continue;
    }
    if (line.match(/^\s*:(param|returns|throws|rtype)\b/)) {
      out.push(formatField(line));
      i++;
      continue;
    }
    if (line.trim() === "|only_mutool|") {
      out.push("**mutool run only**");
      i++;
      continue;
    }
    if (line.trim() === "|only_mupdfjs|") {
      out.push("**mupdf.js only**");
      i++;
      continue;
    }
    if (line.trim() === "|no_new|") {
      out.push("*(not constructible with `new`)*");
      i++;
      continue;
    }
    if (line.trim() === "|interface_type|") {
      out.push("*(interface type)*");
      i++;
      continue;
    }
    out.push(convertInline(line));
    i++;
  }
  return out;
}

function apiHeading(sig: string): string {
  return `#### ${convertInline(sig)}`;
}

function rstToMarkdown(content: string, titleOffset: number): string {
  const lines = content.split(/\r?\n/);
  const out: string[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];

    if (isSkippableTopDirective(line)) {
      i++;
      while (
        i < lines.length &&
        (lines[i].startsWith("\t") || (lines[i].trim() && lines[i].match(/^\s{2,}/)))
      ) {
        i++;
      }
      continue;
    }

    if (isSkippableDirective(line)) {
      i = skipDirectiveOptions(lines, i + 1);
      continue;
    }

    if (i + 1 < lines.length && line.trim()) {
      const ul = lines[i + 1];
      if (isHeadingUnderline(ul)) {
        const hLevel = ul[0] === "-" ? 2 + titleOffset : 1 + titleOffset;
        out.push(`${"#".repeat(hLevel)} ${convertInline(line.trim())}`);
        out.push("");
        i += 2;
        continue;
      }
    }

    const dirMatch = line.match(/^\.\. (\w+)::\s*(.*)$/);
    if (dirMatch) {
      const kind = dirMatch[1];
      const sig = dirMatch[2].trim();
      if (["function", "method", "class", "attribute", "data"].includes(kind)) {
        i++;
        const { block, next } = collectIndentedBlock(lines, i);
        i = next;
        out.push(apiHeading(sig));
        out.push("");
        out.push(...processBody(block));
        out.push("");
        continue;
      }
    }

    if (line.match(/^\.\. code-block::/)) {
      const { md, next } = renderCodeBlock(lines, i);
      out.push(...md);
      i = next;
      continue;
    }

    if (line.match(/^\.\. list-table::/)) {
      const { md, next } = parseListTable(lines, i);
      out.push(...md);
      i = next;
      continue;
    }

    if (line.match(/^\.\. table::/)) {
      const { md, next } = parseGridTable(lines, i);
      out.push(...md);
      i = next;
      continue;
    }

    if (!line.trim()) {
      out.push("");
      i++;
      continue;
    }

    if (line.match(/^\s*:(param|returns|throws|rtype)\b/)) {
      out.push(formatField(line));
      i++;
      continue;
    }

    out.push(convertInline(line));
    i++;
  }

  return out.join("\n");
}

function readRst(name: string): string {
  return readFileSync(join(refDir, name), "utf-8");
}

function readTypeRst(typeName: string): string {
  return readFileSync(join(refDir, "types", `${typeName}.rst`), "utf-8");
}

function extractTypeTitle(rst: string): string {
  const lines = rst.split(/\r?\n/);
  for (let i = 0; i < lines.length - 1; i++) {
    if (isHeadingUnderline(lines[i + 1]) && lines[i].trim()) {
      return lines[i].trim();
    }
  }
  return "";
}

function buildContents(): string {
  const items: string[] = [
    "- [Introduction](#introduction)",
    "- [Functions](#functions)",
    "- [Types](#types)",
  ];
  for (const t of commonTypes) {
    const title = extractTypeTitle(readTypeRst(t)) || t;
    items.push(`  - [${title}](#${slugify(title)})`);
  }
  for (const t of pdfTypes) {
    const title = extractTypeTitle(readTypeRst(t)) || t;
    items.push(`  - [${title}](#${slugify(title)})`);
  }
  return items.join("\n");
}

function main(): void {
  const parts: string[] = [];

  parts.push("# JavaScript API reference for SumatraPDF run");
  parts.push("");
  parts.push(
    "**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**",
  );
  parts.push("");
  parts.push(
    "API reference for [SumatraPDF run](Tool-run.md). The scripting engine is the same as [`mutool run`](https://mupdf.readthedocs.io/en/latest/tools/mutool-run.html); this page is adapted from the [MuPDF JavaScript reference](https://mupdf.readthedocs.io/en/latest/reference/javascript/index.html).",
  );
  parts.push("");
  parts.push("See also [JavaScript examples](Tool-run-javascript-examples.md).");
  parts.push("");
  parts.push("## Contents");
  parts.push("");
  parts.push(buildContents());
  parts.push("");

  parts.push(rstToMarkdown(readRst("introduction.rst"), 1));
  parts.push("");
  parts.push(rstToMarkdown(readRst("functions.rst"), 1));
  parts.push("");
  parts.push("## Types");
  parts.push("");
  parts.push("### Common");
  parts.push("");

  for (const typeName of commonTypes) {
    parts.push(rstToMarkdown(readTypeRst(typeName), 2));
    parts.push("");
  }

  parts.push("### PDF");
  parts.push("");

  for (const typeName of pdfTypes) {
    parts.push(rstToMarkdown(readTypeRst(typeName), 2));
    parts.push("");
  }

  const text = parts.join("\n").replace(/\n{3,}/g, "\n\n") + "\n";
  const tmpPath = outPath + ".tmp";
  writeFileSync(tmpPath, text);
  try {
    renameSync(tmpPath, outPath);
  } catch {
    // Target may be locked (e.g. open in editor); overwrite in place.
    writeFileSync(outPath, text);
    try {
      unlinkSync(tmpPath);
    } catch {
      /* ignore */
    }
  }
  console.log(`wrote ${outPath} (${text.length} bytes)`);
}

if (import.meta.main) {
  main();
}