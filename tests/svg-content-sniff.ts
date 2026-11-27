// An SVG without an .svg extension opens as SVG, found by its content: the
// root element is <svg> after an optional BOM, XML declaration, comments and
// DOCTYPE (Adobe Illustrator's declares entities, which MuPDF's own content
// check stops at). A .txt file that starts with <svg> still opens as text.
//
// Run:  bun tests/svg-content-sniff.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const RECT = `<rect width="200" height="100" fill="red"/>`;
const PLAIN = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">${RECT}</svg>`;
const ILLUSTRATOR =
  "﻿" +
  `<?xml version="1.0" encoding="utf-8"?>\n<!-- Generator: Adobe Illustrator 10.0 -->\n` +
  `<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.0//EN" "http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd" [\n` +
  `\t<!ENTITY ns_svg "http://www.w3.org/2000/svg">\n]>\n` +
  `<svg xmlns="&ns_svg;" width="200" height="100">${RECT}</svg>`;

interface Rendered {
  width: number;
  height: number;
}

function render(path: string): Rendered | string {
  const out = tmpPath("svg-content-sniff.png");
  const r = Bun.spawnSync([EXE, "cli", "render", path, "-o", out, "--json"]);
  const text = r.stdout.toString() + r.stderr.toString();
  try {
    const j = JSON.parse(text);
    return j.images?.[0] ?? text;
  } catch {
    return text;
  }
}

export async function testit(): Promise<void> {
  const bad: string[] = [];
  for (const [name, content] of [
    ["svg-content-sniff-plain", PLAIN],
    ["svg-content-sniff-illustrator", ILLUSTRATOR],
  ]) {
    const path = tmpPath(name); // no extension
    writeFileSync(path, content);
    const got = render(path);
    // 200x100 pt at the default 150 dpi
    if (typeof got === "string" || got.width !== 417 || got.height !== 209) {
      bad.push(`${name}: ${JSON.stringify(got)}, want 417x209`);
    }
  }
  const txt = tmpPath("svg-content-sniff.txt");
  writeFileSync(txt, `${PLAIN}\n\nsome text\n`);
  const got = render(txt);
  if (typeof got === "string" || got.width === 417) {
    bad.push(`a .txt starting with <svg> isn't shown as text: ${JSON.stringify(got)}`);
  }
  if (bad.length > 0) {
    throw new Error(`svg-content-sniff:\n${bad.join("\n")}`);
  }
  console.log("svg-content-sniff: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
