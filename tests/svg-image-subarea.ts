// A scalable (SVG / display-list) image that is only partly visible, e.g. in
// one tile of a zoomed page or running off the page, is drawn as just that
// part. mupdf rendered the part but drew it over the whole image's rectangle,
// so it came out stretched and in the wrong place
// (ext/patches/0097-scalable-image-subarea-ctm).
//
// Run:  bun tests/svg-image-subarea.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util";

// four vertical stripes, 50 px each: red, green, blue, yellow
const STRIPES =
  '<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200">' +
  '<rect x="0" width="50" height="200" fill="#ff0000"/><rect x="50" width="50" height="200" fill="#00ff00"/>' +
  '<rect x="100" width="50" height="200" fill="#0000ff"/><rect x="150" width="50" height="200" fill="#ffff00"/>' +
  "</svg>";

// the SVG fills a 200x200 page
const HTML =
  "<html><head><style>@page{margin:0} body{margin:0} img{display:block;width:200pt;height:200pt}</style></head>" +
  '<body><img src="svg-image-subarea-stripes.svg"></body></html>';

// Draws the page into a pixmap covering only its right half (x 100..200), so
// only the right half of the image is needed, and prints the color at x=125
// (blue stripe) and x=175 (yellow stripe).
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]);
doc.layout(200, 200, 12);
var page = doc.loadPage(0);
var pix = new Pixmap(ColorSpace.DeviceRGB, [100, 0, 200, 200], false);
pix.clear(255);
var dev = new DrawDevice(Matrix.identity, pix);
page.run(dev, Matrix.identity);
dev.close();
function rgb(x) {
  return [pix.getSample(x - 100, 100, 0), pix.getSample(x - 100, 100, 1), pix.getSample(x - 100, 100, 2)].join(",");
}
print("at125=" + rgb(125));
print("at175=" + rgb(175));
`;

export async function testit(): Promise<void> {
  const html = tmpPath("svg-image-subarea.html");
  const script = tmpPath("svg-image-subarea.js");
  writeFileSync(join(html, "..", "svg-image-subarea-stripes.svg"), STRIPES);
  writeFileSync(html, HTML);
  writeFileSync(script, SCRIPT);

  // mupdf finds the image next to the page only with forward slashes
  const r = Bun.spawnSync([EXE, "run", script, html.replaceAll("\\", "/")]);
  const out = r.stdout.toString() + r.stderr.toString();
  const at125 = /^at125=(.*)$/m.exec(out)?.[1];
  const at175 = /^at175=(.*)$/m.exec(out)?.[1];
  if (r.exitCode !== 0 || at125 === undefined || at175 === undefined) {
    throw new Error(`svg-image-subarea: exit ${r.exitCode}\n${out}`);
  }
  if (at125 !== "0,0,255" || at175 !== "255,255,0") {
    throw new Error(
      `svg-image-subarea: right half drawn wrong: x=125 ${at125} (want blue), x=175 ${at175} (want yellow)`,
    );
  }
  console.log("svg-image-subarea: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
