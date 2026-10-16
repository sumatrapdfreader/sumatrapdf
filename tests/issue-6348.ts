// #6348: a Cambria Math subset whose FontDescriptor ascent/descent is the
// font-wide bbox (3117 / -2464, about 5.6em) made every glyph that tall, so
// the selection bar covered the next line. Glyph ink is used only when the
// font's own metric box is taller than 3em. A second copy of the same font
// with Ascent 800 / Descent -200 must keep that 1em box, including a period
// whose ink is much shorter.
//
// Fixture: tests/issue-6348.pdf, built by tests/issue-6348-make.ts.
//
// Run:  bun tests/issue-6348.ts [--no-build]

import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-6348.pdf");

function heightsOf(boxes: number[][], cp: number): number[] {
  return boxes.filter((b) => b[0] === cp).map((b) => b[1]);
}

export async function testit(): Promise<void> {
  const p = Bun.spawnSync([EXE, "-for-testing", "-extract-text", "1", PDF], { stdout: "pipe", stderr: "pipe" });
  const raw = p.stdout.toString() + p.stderr.toString();
  const m = /boxh on page 1:(.*)/.exec(raw);
  if (!m) {
    throw new Error(`no boxh line (debug build?). out=${raw.slice(0, 500).replace(/\s+/g, " ")}`);
  }
  const boxes = m[1]
    .trim()
    .split(/\s+/)
    .filter((s) => s.length > 0)
    .map((s) => {
      const [cp, dy] = s.split(":").map((n) => parseInt(n, 10));
      return [cp, dy];
    });

  const dots = heightsOf(boxes, 46);
  const parens = heightsOf(boxes, 40);
  const aitches = heightsOf(boxes, 72);
  if (dots.length < 2 || parens.length < 2 || aitches.length < 1) {
    throw new Error(`expected two periods, two parens, one H; got ${m[1].trim()}`);
  }

  // The tall descriptor must not survive as a ~73pt bar.
  for (const dy of boxes.map((b) => b[1])) {
    if (dy > 30) {
      throw new Error(`glyph box still uses the font bbox: ${m[1].trim()}`);
    }
  }
  // Period ink is a few pt. The 1em copy of the same glyph stays on the font box.
  const shortDot = Math.min(...dots);
  const metricDot = Math.max(...dots);
  if (shortDot >= 8) {
    throw new Error(`tall-font period did not use glyph ink: ${dots.join(",")}`);
  }
  if (metricDot < 10 || metricDot > 18) {
    throw new Error(`1em font period changed size: ${dots.join(",")}`);
  }
  for (const dy of parens) {
    if (dy < 8 || dy > 20) {
      throw new Error(`paren box out of range: ${parens.join(",")}`);
    }
  }
  if (aitches[0] < 8 || aitches[0] > 18) {
    throw new Error(`Helvetica H changed size: ${aitches[0]}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
