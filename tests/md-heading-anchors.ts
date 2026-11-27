// In mupdf's Markdown rendering (the fixed-page view, cli render / text), a
// link to a heading's GitHub-style anchor ("#café-au-lait", "#привет-мир")
// goes to the page the heading is on. autoheaderid.c skipped a byte after
// every 2-byte UTF-8 character and wrote non-ASCII ones as single bytes, so
// such anchors never matched; and it wrapped the heading in <a id>, which
// could land on the previous page when the heading moved to the next.
// (ext/cmark-gfm, see ext/versions.txt; MarkdownHeadingSlug in MarkdownToc.cpp
// makes the same anchors for the WebView2 view.)
//
// Run:  bun tests/md-heading-anchors.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const TITLES = ["Section %d", "Café au lait %d", "Привет мир %d", "Ünïcödé Ärger %d"];

// GitHub's slug: lowercase ASCII, keep other letters, drop punctuation, a
// space becomes '-'
function slug(title: string): string {
  let out = "";
  for (const ch of title) {
    if (ch >= "A" && ch <= "Z") out += ch.toLowerCase();
    else if (/[a-z0-9_-]/.test(ch) || ch.codePointAt(0)! >= 0x80) out += ch;
    else if (ch === " ") out += "-";
  }
  return out;
}

function makeMd(): { md: string; anchors: { title: string; anchor: string }[] } {
  const parts: string[] = [];
  const anchors: { title: string; anchor: string }[] = [];
  for (let i = 1; i <= 40; i++) {
    const title = TITLES[i % TITLES.length]!.replace("%d", String(i));
    anchors.push({ title, anchor: slug(title) });
    parts.push(`## ${title}\n`);
    // varying lengths so that some headings land at a page break
    parts.push(`${"Some filler text for this section. ".repeat(8 + ((i * 7) % 23))}\n`);
  }
  return { md: parts.join("\n"), anchors };
}

// prints "anchor|link page|page with the heading's text" per heading
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]);
var anchors = JSON.parse(scriptArgs[1]);
var texts = [];
for (var i = 0; i < doc.countPages(); i++)
  texts.push(doc.loadPage(i).toStructuredText().asText().replace(/\\s+/g, " "));
for (var j = 0; j < anchors.length; j++) {
  var a = anchors[j];
  var linkPage = -1;
  try { linkPage = doc.resolveLink("#" + a.anchor); } catch (e) {}
  var textPage = -1;
  for (var p = 0; p < texts.length && textPage < 0; p++)
    if (texts[p].indexOf(a.title) >= 0) textPage = p;
  print(a.anchor + "|" + linkPage + "|" + textPage);
}
`;

export async function testit(): Promise<void> {
  const { md, anchors } = makeMd();
  const mdPath = tmpPath("md-heading-anchors.md");
  const script = tmpPath("md-heading-anchors.js");
  writeFileSync(mdPath, md);
  writeFileSync(script, SCRIPT);
  const r = Bun.spawnSync([EXE, "run", script, mdPath.replaceAll("\\", "/"), JSON.stringify(anchors)]);
  const out = r.stdout.toString() + r.stderr.toString();
  const rows = [...out.matchAll(/^(.+)\|(-?\d+)\|(-?\d+)$/gm)];
  if (r.exitCode !== 0 || rows.length !== anchors.length) {
    throw new Error(`md-heading-anchors: exit ${r.exitCode}, ${rows.length} rows\n${out}`);
  }
  const bad = rows
    .filter((m) => m[2] !== m[3] || m[3] === "-1")
    .map((m) => `#${m[1]}: link page ${m[2]}, heading on ${m[3]}`);
  if (bad.length > 0) {
    throw new Error(`md-heading-anchors: ${bad.length} of ${rows.length} wrong:\n${bad.slice(0, 8).join("\n")}`);
  }
  console.log("md-heading-anchors: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
