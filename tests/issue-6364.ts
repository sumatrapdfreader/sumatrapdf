// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6364
// SVG pages in a comic book archive (.cbz) open and render.

import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, writeStoredZip } from "./util.ts";

function redSvg(dx: number, dy: number): Buffer {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${dx}" height="${dy}">
<rect x="0" y="0" width="${dx}" height="${dy}" fill="#ff0000"/>
</svg>
`;
  return Buffer.from(svg, "utf8");
}

export async function testit(): Promise<void> {
  const cbz = tmpPath("issue-6364.cbz");
  writeStoredZip(cbz, [
    { name: "01.svg", data: redSvg(200, 300) },
    { name: "02.svg", data: redSvg(300, 200) },
  ]);

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestRenderPageColors, [cbz]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6364: ${raw.trim()}`);
    }
    const m = /red=(\d+) nonwhite=(\d+) size=(\d+)x(\d+) pages=(\d+)/.exec(raw);
    if (!m) {
      throw new Error(`issue-6364: could not parse: ${raw}`);
    }
    const [red, w, h, pages] = [+m[1]!, +m[3]!, +m[4]!, +m[5]!];
    if (pages !== 2) {
      throw new Error(`issue-6364: pages=${pages}, want 2:\n${raw}`);
    }
    if (w !== 200 || h !== 300) {
      throw new Error(`issue-6364: size=${w}x${h}, want 200x300:\n${raw}`);
    }
    if (red < 100) {
      throw new Error(`issue-6364: page 1 is not red (red=${red}):\n${raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
