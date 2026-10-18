// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6339
//
// SVG text ignored rgba() fills (drew black) and @font-face fonts embedded as
// WOFF data URIs (drew in Times). The font is NotoSans wrapped in WOFF here;
// its "m" is much wider than Times', so the text box width tells them apart.
//
// Run: bun tests/issue-6339.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const FONT = join(import.meta.dir, "..", "ext", "mupdf", "resources", "fonts", "noto", "NotoSans-Regular.otf");
const TEXT = "mmmmmmmmmm";

const align4 = (n: number) => (n + 3) & ~3;

// Wrap an sfnt (TTF/OTF) in WOFF 1.0: a header, a table directory and zlib-compressed tables.
function toWoff(sfnt: Buffer): Buffer {
  const numTables = sfnt.readUInt16BE(4);
  const tables = [];
  for (let i = 0; i < numTables; i++) {
    const e = 12 + i * 16;
    const offset = sfnt.readUInt32BE(e + 8);
    const length = sfnt.readUInt32BE(e + 12);
    const orig = sfnt.subarray(offset, offset + length);
    const comp = deflateSync(orig);
    tables.push({
      tag: sfnt.readUInt32BE(e),
      checksum: sfnt.readUInt32BE(e + 4),
      orig,
      data: comp.length < orig.length ? comp : orig,
    });
  }

  let offset = 44 + numTables * 20;
  let sfntSize = 12 + numTables * 16;
  const offsets = tables.map((t) => {
    const o = offset;
    offset += align4(t.data.length);
    sfntSize += align4(t.orig.length);
    return o;
  });

  const out = Buffer.alloc(offset);
  out.write("wOFF", 0, "latin1");
  out.writeUInt32BE(sfnt.readUInt32BE(0), 4);
  out.writeUInt32BE(offset, 8);
  out.writeUInt16BE(numTables, 12);
  out.writeUInt32BE(sfntSize, 16);
  out.writeUInt16BE(1, 20);
  tables.forEach((t, i) => {
    const d = 44 + i * 20;
    out.writeUInt32BE(t.tag, d);
    out.writeUInt32BE(offsets[i]!, d + 4);
    out.writeUInt32BE(t.data.length, d + 8);
    out.writeUInt32BE(t.orig.length, d + 12);
    out.writeUInt32BE(t.checksum, d + 16);
    t.data.copy(out, offsets[i]!);
  });
  return out;
}

function svg(style: string, fill: string): string {
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="600" height="100" viewBox="0 0 600 100">` +
    `<style>${style}</style>` +
    `<text x="10" y="60" font-family="emb" font-size="40" fill="${fill}">${TEXT}</text></svg>`
  );
}

async function render(client: ControlClient, name: string, content: string) {
  const path = tmpPath(name);
  writeFileSync(path, content);
  const res = await client.request(ControlCommand.TestRenderPageColors, [path, 1, 100]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6339: ${name}: render failed: ${raw}`);
  }
  const b = (/box=([\d,]+)/.exec(raw)?.[1] ?? "0,0,0,0").split(",").map(Number);
  const red = Number(/red=(\d+)/.exec(raw)?.[1] ?? 0);
  const nonWhite = Number(/nonwhite=(\d+)/.exec(raw)?.[1] ?? 0);
  console.log(`issue-6339: ${name}: ${raw}`);
  return { width: b[2]! - b[0]!, red, nonWhite };
}

export async function testit(): Promise<void> {
  const woff = toWoff(readFileSync(FONT)).toString("base64");
  const fontFace = `@font-face { font-family: "emb"; font-weight: 400; src: url(data:application/x-font-woff;charset=utf-8;base64,${woff}); }`;

  await withControlledSumatra(EXE, async (client) => {
    const colored = await render(client, "issue-6339-rgba.svg", svg("", "rgba(255,0,0,1)"));
    if (colored.red < colored.nonWhite / 3) {
      throw new Error(`issue-6339: rgba() fill was not red: red=${colored.red} nonwhite=${colored.nonWhite}`);
    }

    const fallback = await render(client, "issue-6339-fallback.svg", svg("", "#000"));
    const embedded = await render(client, "issue-6339-font.svg", svg(fontFace, "#000"));
    if (embedded.nonWhite < 100 || embedded.width - fallback.width < 40) {
      throw new Error(`issue-6339: @font-face font not used: width ${embedded.width} vs fallback ${fallback.width}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
