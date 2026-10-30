// An image's page element (what double-click selects and Ctrl+C copies) must
// cover only its visible part: cut by the clip path and the page (issue #6441).
//
// Run: bun tests/issue-6441.ts [--no-build] [-ng]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

// page rects (x, y from top-left, dx, dy) the images must have
const kExpected = [
  "100,392,300,200", // 500x700 image clipped to a 300x200 rect
  "500,0,112,92", // 200x200 image sticking out of the page's top-right corner
];

function makePdf(): string {
  const hex = "ff0000".repeat(4) + "0000ff";
  const content = "q 100 200 300 200 re W n 500 0 0 700 56 46 cm /Im0 Do Q q 200 0 0 200 500 700 cm /Im0 Do Q";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /XObject /Subtype /Image /Width 1 /Height 5 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /ASCIIHexDecode /Length ${hex.length + 1} >>\nstream\n${hex}>\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6441.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled([]);
  try {
    const res = await client.request(ControlCommand.TestPageImages, [pdf, 1]);
    const out = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6441: TestPageImages failed: ${out}`);
    }
    const got = [...out.matchAll(/image rect=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)/g)].map((m) =>
      m
        .slice(1, 5)
        .map((v) => Math.round(+v))
        .join(","),
    );
    got.sort();
    if (got.join(" ") !== [...kExpected].sort().join(" ")) {
      throw new Error(`issue-6441: image rects are ${got.join(" ")}, want ${kExpected.join(" ")}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6441: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
