// An SVG <image> keeps its aspect ratio: preserveAspectRatio defaults to
// "xMidYMid meet" (fit inside the box, centered); "none" stretches; "slice"
// fills the box and is clipped to it. mupdf stretched every image to its box,
// so e.g. a cover image in an EPUB came out squashed
// (ext/patches/0110-svg-image-preserve-aspect-ratio).
//
// Run:  bun tests/svg-image-aspect.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { EXE, runStandalone, tmpPath } from "./util";

// a w x h PNG of one RGB color
function solidPng(w: number, h: number, rgb: [number, number, number]): Buffer {
  const chunk = (type: string, data: Buffer) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type, "latin1"), data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(Bun.hash.crc32(td) >>> 0);
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 2; // RGB
  const row = Buffer.concat([Buffer.from([0]), Buffer.from(Array(w).fill(rgb).flat())]);
  const raw = Buffer.concat(Array(h).fill(row));
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", ihdr),
    chunk("IDAT", deflateSync(raw)),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

// red at x=25, 75, 125, 175 (y=50) of a 200x100 page where a square red image
// fills a 200x100 box with the given preserveAspectRatio ("" = the default)
const CASES: { par: string; want: string }[] = [
  { par: "", want: "0,1,1,0" }, // meet, centered: x 50..150
  { par: "xMidYMid meet", want: "0,1,1,0" },
  { par: "xMinYMid meet", want: "1,1,0,0" }, // x 0..100
  { par: "xMaxYMid", want: "0,0,1,1" }, // x 100..200
  { par: "none", want: "1,1,1,1" }, // stretched
  { par: "xMidYMid slice", want: "1,1,1,1" }, // 200x200, clipped to the box
];

const SCRIPT = `
var paths = JSON.parse(scriptArgs[0]);
for (var i = 0; i < paths.length; i++) {
  var p = Document.openDocument(paths[i]).loadPage(0).toPixmap(Matrix.identity, ColorSpace.DeviceRGB, false);
  var red = [25, 75, 125, 175].map(function (x) {
    return p.getSample(x, 50, 0) > 200 && p.getSample(x, 50, 1) < 60 ? 1 : 0;
  });
  print("case" + i + "=" + red.join(","));
}
`;

export async function testit(): Promise<void> {
  const png = solidPng(40, 40, [255, 0, 0]).toString("base64");
  const paths: string[] = [];
  CASES.forEach((c, i) => {
    const par = c.par ? ` preserveAspectRatio="${c.par}"` : "";
    const svg =
      `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="200" height="100">` +
      `<rect width="200" height="100" fill="white"/>` +
      `<image x="0" y="0" width="200" height="100"${par} xlink:href="data:image/png;base64,${png}"/></svg>`;
    const path = tmpPath(`svg-image-aspect-${i}.svg`);
    writeFileSync(path, svg);
    paths.push(path.replaceAll("\\", "/"));
  });
  const script = tmpPath("svg-image-aspect.js");
  writeFileSync(script, SCRIPT);
  const r = Bun.spawnSync([EXE, "run", script, JSON.stringify(paths)]);
  const out = r.stdout.toString() + r.stderr.toString();
  const bad: string[] = [];
  CASES.forEach((c, i) => {
    const got = new RegExp(`^case${i}=(\\S+)$`, "m").exec(out)?.[1];
    if (got !== c.want) {
      bad.push(`preserveAspectRatio="${c.par || "(default)"}": red at x=25,75,125,175 is ${got}, want ${c.want}`);
    }
  });
  if (r.exitCode !== 0 || bad.length > 0) {
    throw new Error(`svg-image-aspect: exit ${r.exitCode}\n${bad.join("\n")}\n${out}`);
  }
  console.log("svg-image-aspect: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
