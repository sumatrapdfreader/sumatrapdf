// A JPEG tile rendered after a full-page render showed the whole image
// squeezed into the tile (a patchwork at high zoom): mupdf hands back its
// cached full decode for a sub-rect request and the engine scaled it as-is
// (discussion #6229).
//
// Run: bun tests/issue-6229.ts [--no-build]
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

// 400x400, left quarter red, rest blue; regenerate with tests/issue-6229-make-fixture.py
const JPG = join(dirname(fileURLToPath(import.meta.url)), "issue-6229.jpg");
const W = 400;
const RIGHT_HALF_TILE = 2;

function parseEdges(raw: string): { w: number; h: number; left: number[]; right: number[] } {
  const m = /size=(\d+)x(\d+) left=(\d+),(\d+),(\d+) right=(\d+),(\d+),(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6229: could not parse: ${raw}`);
  }
  return { w: +m[1]!, h: +m[2]!, left: [+m[3]!, +m[4]!, +m[5]!], right: [+m[6]!, +m[7]!, +m[8]!] };
}

function isBlue(c: number[]): boolean {
  return c[2]! > 150 && c[0]! < 80 && c[1]! < 80;
}

export async function testit(): Promise<void> {
  await withControlledSumatra(EXE, async (client) => {
    for (const zoom of [200, 400]) {
      const res = await client.request(ControlCommand.TestImageRenderEdges, [JPG, zoom, RIGHT_HALF_TILE]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-6229 ${zoom}%: ${raw.trim()}`);
      }
      const e = parseEdges(raw);
      const wantW = (W / 2) * (zoom / 100);
      const wantH = W * (zoom / 100);
      if (e.w !== wantW || e.h !== wantH) {
        throw new Error(`issue-6229 ${zoom}%: tile ${e.w}x${e.h}, want ${wantW}x${wantH}:\n${raw}`);
      }
      // the right half of the image is all blue; red on the left means the
      // whole image was squeezed into the tile
      if (!isBlue(e.left)) {
        throw new Error(`issue-6229 ${zoom}%: tile left edge ${e.left.join(",")} is not blue:\n${raw}`);
      }
      if (!isBlue(e.right)) {
        throw new Error(`issue-6229 ${zoom}%: tile right edge ${e.right.join(",")} is not blue:\n${raw}`);
      }
      console.log(`  ${zoom}%: right-half tile ${e.w}x${e.h} all blue ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
