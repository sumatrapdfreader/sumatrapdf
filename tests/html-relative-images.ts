// An HTML page's relative images (<img src="pic.svg">) are found next to the
// page:
// - by mupdf given a Windows path with backslashes: fz_dirname() only split
//   on '/', so the page's folder was "." (ext/patches/0100-dirname-backslash)
// - by our engine (cli render, thumbnails, fixed-page view), which opened an
//   HTML file as a stream with no folder, so no relative image ever loaded
//
// Run:  bun tests/html-relative-images.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util";

// a page that is one solid blue image
const IMG =
  '<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200"><rect width="200" height="200" fill="#0000ff"/></svg>';
const HTML = '<html><body><img src="html-relative-images.svg" style="width:200pt;height:200pt"></body></html>';

// counts blue pixels (sampled) on the first page of a document or image
const SCRIPT = `
var p = Document.openDocument(scriptArgs[0]).loadPage(0).toPixmap(Matrix.identity, ColorSpace.DeviceRGB, false);
var blue = 0;
for (var y = 0; y < p.getHeight(); y += 4)
  for (var x = 0; x < p.getWidth(); x += 4)
    if (p.getSample(x, y, 2) > 200 && p.getSample(x, y, 0) < 60) blue++;
print("blue=" + blue);
`;

function blueSamples(script: string, path: string): number {
  const r = Bun.spawnSync([EXE, "run", script, path]);
  const out = r.stdout.toString() + r.stderr.toString();
  const m = /^blue=(\d+)$/m.exec(out);
  if (r.exitCode !== 0 || !m) {
    throw new Error(`html-relative-images: run ${path}: exit ${r.exitCode}\n${out}`);
  }
  return Number(m[1]);
}

export async function testit(): Promise<void> {
  const html = tmpPath("html-relative-images.html"); // a Windows path, with backslashes
  const script = tmpPath("html-relative-images.js");
  const png = tmpPath("html-relative-images.png");
  writeFileSync(join(html, "..", "html-relative-images.svg"), IMG);
  writeFileSync(html, HTML);
  writeFileSync(script, SCRIPT);

  const failed: string[] = [];
  if (blueSamples(script, html) === 0) {
    failed.push(`mupdf (run) didn't find the image next to ${html}`);
  }

  const r = Bun.spawnSync([EXE, "cli", "render", html, "-o", png]);
  if (r.exitCode !== 0) {
    throw new Error(`html-relative-images: cli render: exit ${r.exitCode}\n${r.stderr.toString()}`);
  }
  if (blueSamples(script, png) === 0) {
    failed.push("cli render of the page has no image");
  }
  if (failed.length > 0) {
    throw new Error(`html-relative-images:\n${failed.join("\n")}`);
  }
  console.log("html-relative-images: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
