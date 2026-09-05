// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4315
//
// KF8 / AZW3 fixed-layout books store pages as JPEGs in the PDB and leave the
// PalmDoc HTML empty. EngineMobi must still show those images as pages.
//
// Fixture: tests/issue-4315.azw3 (two solid-color JPEG pages).
// Regenerate with: bun tests/issue-4315-make.ts
//
// Run: bun tests/issue-4315.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

const AZW3 = join(import.meta.dir, "issue-4315.azw3");

function parseColors(raw: string): { red: number; blue: number } {
  const red = /red=(\d+)/.exec(raw);
  const blue = /blue=(\d+)/.exec(raw);
  return { red: red ? +red[1]! : 0, blue: blue ? +blue[1]! : 0 };
}

export async function testit(): Promise<void> {
  if (!existsSync(AZW3)) {
    throw new Error(`issue-4315: fixture not found: ${AZW3}`);
  }

  await withControlledSumatra(EXE, async (client) => {
    const p1 = await client.request(ControlCommand.TestRenderPageColors, [AZW3, 1]);
    const p1Raw = String(p1[1] ?? "");
    if (p1[0] !== 0) {
      throw new Error(`issue-4315 page 1: ${p1Raw.trim()}`);
    }
    const c1 = parseColors(p1Raw);
    if (c1.red < 100 || c1.red <= c1.blue) {
      throw new Error(`issue-4315: page 1 should be red, got red=${c1.red} blue=${c1.blue}`);
    }

    const p2 = await client.request(ControlCommand.TestRenderPageColors, [AZW3, 2]);
    const p2Raw = String(p2[1] ?? "");
    if (p2[0] !== 0) {
      throw new Error(`issue-4315 page 2: ${p2Raw.trim()}`);
    }
    const c2 = parseColors(p2Raw);
    if (c2.blue < 100 || c2.blue <= c2.red) {
      throw new Error(`issue-4315: page 2 should be blue, got red=${c2.red} blue=${c2.blue}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
