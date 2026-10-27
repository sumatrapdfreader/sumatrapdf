// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6399
//
// AZW3 fixed-layout books are shown as one image per page. The page must have
// the image's size and no border, or facing pages show a white gap.
//
// Fixture: tests/issue-4315.azw3 (two 400x600 JPEG pages).
//
// Run: bun tests/issue-6399.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

const AZW3 = join(import.meta.dir, "issue-4315.azw3");
const kNotReady = 2;

function parseBox(raw: string, name: string): number[] {
  const m = new RegExp(`${name}=([-\\d.]+),([-\\d.]+),([-\\d.]+),([-\\d.]+)`).exec(raw);
  if (!m) {
    throw new Error(`issue-6399: no ${name} in: ${raw.trim()}`);
  }
  return [+m[1]!, +m[2]!, +m[3]!, +m[4]!];
}

export async function testit(): Promise<void> {
  if (!existsSync(AZW3)) {
    throw new Error(`issue-6399: fixture not found: ${AZW3}`);
  }

  await withControlledSumatra(
    EXE,
    async (client) => {
      let raw = "";
      const deadline = Date.now() + 10_000;
      for (;;) {
        const res = await client.request(ControlCommand.TestPageBoxes, [1]);
        raw = String(res[1] ?? "");
        if (res[0] === 0) {
          break;
        }
        if (res[0] !== kNotReady || Date.now() > deadline) {
          throw new Error(`issue-6399: TestPageBoxes failed: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 50));
      }

      const media = parseBox(raw, "engineMedia");
      const content = parseBox(raw, "engineContent");
      const ratio = media[2]! / media[3]!;
      if (Math.abs(ratio - 400 / 600) > 0.01) {
        throw new Error(`issue-6399: page should have the image's 2:3 shape, got ${media.join(",")}`);
      }
      if (content.join(",") !== media.join(",")) {
        throw new Error(
          `issue-6399: page should have no border, media=${media.join(",")} content=${content.join(",")}`,
        );
      }
    },
    [AZW3],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
