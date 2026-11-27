// The table of contents of an HTML / FB2 / MOBI document points each entry at
// its heading. It was resolved with one walk of the document per entry, on
// every layout, and again when the bookmarks were built (EngineMupdf
// ResolveLink per entry), so a book with thousands of chapters took seconds to
// open. fz_find_html_targets resolves all entries in one walk
// (ext/patches/0094-html-outline-targets-one-walk) and the bookmarks use those
// results. Every entry must still point at the page its heading is on.
//
// Run:  bun tests/html-toc-targets.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";

// Each heading has a unique token Q<i>Z to find it in the page text. Headings
// with explicit ids, generated ids, ids on a wrapping div and on an inline
// span, nested levels, text between them so chapters span pages.
function makeHtml(chapters: number): string {
  const parts: string[] = ["<html><body>"];
  const para = "<p>" + "Some text to fill the page. ".repeat(20) + "</p>";
  for (let i = 1; i <= chapters; i++) {
    const title = `Heading Q${i}Z`;
    switch (i % 4) {
      case 0:
        parts.push(`<h1 id="ch${i}">${title}</h1>`);
        break;
      case 1:
        parts.push(`<div id="wrap${i}"><h2>${title}</h2></div>`);
        break;
      case 2:
        parts.push(`<h2><span id="sp${i}">Heading</span> Q${i}Z</h2>`);
        break;
      default:
        parts.push(`<h3>${title}</h3>`);
    }
    parts.push(para.repeat(1 + (i % 3)));
  }
  parts.push("</body></html>");
  return parts.join("\n");
}

type TocEntry = { title: string; page: number; children?: TocEntry[] };

function flatten(entries: TocEntry[], out: TocEntry[]): TocEntry[] {
  for (const e of entries) {
    out.push(e);
    flatten(e.children ?? [], out);
  }
  return out;
}

function cli(args: string[]): string {
  const r = Bun.spawnSync([EXE, "cli", ...args]);
  if (r.exitCode !== 0) {
    throw new Error(`html-toc-targets: cli ${args.join(" ")}: exit ${r.exitCode}\n${r.stderr.toString()}`);
  }
  return r.stdout.toString();
}

function check(name: string, chapters: number): number {
  const html = tmpPath(`html-toc-targets-${name}.html`);
  writeFileSync(html, makeHtml(chapters));

  const start = performance.now();
  const toc = flatten(JSON.parse(cli(["toc", "--json", html])).toc as TocEntry[], []);
  const ms = performance.now() - start;

  // page (1-based) of each heading's token
  const pageOf = new Map<string, number>();
  cli(["text", html])
    .split("\f")
    .forEach((text, i) => {
      for (const m of text.matchAll(/Q(\d+)Z/g)) {
        if (!pageOf.has(m[1]!)) {
          pageOf.set(m[1]!, i + 1);
        }
      }
    });

  if (toc.length !== chapters) {
    throw new Error(`html-toc-targets ${name}: ${toc.length} entries, want ${chapters}`);
  }
  const bad: string[] = [];
  for (const e of toc) {
    const num = /Q(\d+)Z/.exec(e.title)?.[1] ?? "";
    const want = pageOf.get(num);
    if (want !== e.page) {
      bad.push(`${e.title}: page ${e.page}, heading is on page ${want}`);
    }
  }
  if (bad.length > 0) {
    throw new Error(`html-toc-targets ${name}: ${bad.length} wrong:\n${bad.slice(0, 5).join("\n")}`);
  }
  return ms;
}

export async function testit(): Promise<void> {
  check("small", 40);
  const ms = check("big", 3000);
  // a debug build took 26 s, resolving each entry with a walk of the whole book
  if (ms > 6000 * SLOW_BUILD_FACTOR) {
    throw new Error(`html-toc-targets: toc of 3000 chapters took ${Math.round(ms)} ms`);
  }
  console.log(`html-toc-targets: OK (toc of 3000 chapters: ${Math.round(ms)} ms)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
