// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6322
//
// An SVG data URI may have media-type parameters before `;base64`.
// MuPDF only recognized the exact `data:image/jpeg;base64,` prefix.

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const RED_JPEG = join(import.meta.dir, "issue-6236.jpg");

export async function testit(): Promise<void> {
  const jpeg = readFileSync(RED_JPEG).toString("base64");
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="300">
<image width="200" height="300" href="data:image/jpeg;charset=utf-8;base64,${jpeg}"/>
</svg>`;
  const path = tmpPath("issue-6322.svg");
  writeFileSync(path, svg);

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestRenderPageColors, [path]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6322: render failed: ${raw.trim()}`);
    }
    const red = Number(/red=(\d+)/.exec(raw)?.[1] ?? 0);
    if (red < 200) {
      throw new Error(`issue-6322: parameterized data URI was not rendered: ${raw.trim()}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
