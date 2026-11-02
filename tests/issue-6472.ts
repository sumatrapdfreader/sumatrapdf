// #6472: a link annotation's /QuadPoints are its clickable areas, but we used
// its /Rect, the box around them. Page 1 is the reporter's round link: quads
// listed as a polygon. Page 2 is a link broken over two lines, as PDF/UA-2 asks
// for: one annotation, a quad per line in Acrobat's order (UL, UR, LL, LR).
//
// Run: bun tests/issue-6472.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util.ts";

const PAGE_DY = 375;
const URI = "https://example.com/";

function linkAnnot(rect: number[], quads: number[]): string {
  return `<< /Type /Annot /Subtype /Link /Border [0 0 0] /Rect [${rect.join(" ")}] /QuadPoints [${quads.join(" ")}] /A << /S /URI /URI (${URI}) >> >>`;
}

function makePdf(): string {
  const round = linkAnnot(
    [112.115, 112.115, 262.885, 262.885],
    [
      187.5, 112.125, 262.875, 145.872, 112.125, 229.128, 145.872, 112.125, 229.128, 262.875, 112.125, 229.128, 112.125,
      229.128, 262.875, 145.872,
    ],
  );
  // the end of one line (250..350 at y 300..320), the start of the next (20..80 at y 270..290)
  const twoLines = linkAnnot(
    [20, 270, 350, 320],
    [250, 320, 350, 320, 250, 300, 350, 300, 20, 290, 80, 290, 20, 270, 80, 270],
  );
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 375 ${PAGE_DY}] /Annots [5 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 375 ${PAGE_DY}] /Annots [6 0 R] >>`,
    round,
    twoLines,
  ]);
}

// what is at a point given in PDF coordinates (y up); the page's are y down
type Probe = { page: number; x: number; y: number; want: "link" | "none"; what: string };
const probes: Probe[] = [
  { page: 1, x: 187, y: 187, want: "link", what: "the round link's center" },
  { page: 1, x: 187, y: 117, want: "link", what: "the round link's bottom edge" },
  { page: 1, x: 116, y: 116, want: "none", what: "a corner of the round link's Rect" },
  { page: 1, x: 259, y: 259, want: "none", what: "the opposite corner of its Rect" },
  { page: 2, x: 300, y: 310, want: "link", what: "the link's part on the first line" },
  { page: 2, x: 50, y: 280, want: "link", what: "the link's part on the second line" },
  { page: 2, x: 150, y: 310, want: "none", what: "the first line before the link" },
  { page: 2, x: 200, y: 280, want: "none", what: "the second line after the link" },
];

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6472.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  await withControlledSumatra(EXE, async (client) => {
    const failures: string[] = [];
    for (const p of probes) {
      const res = await client.request(ControlCommand.TestPageLinks, [pdf, p.page, p.x, PAGE_DY - p.y]);
      const raw = String(res[1] ?? "");
      const hit = /hit=(\S+)/.exec(raw)?.[1] ?? "?";
      const got = hit === "dest" ? "link" : hit;
      if (got !== p.want) {
        failures.push(`${p.what} (${p.x},${p.y}): want ${p.want}, got ${hit}`);
      }
    }
    if (failures.length > 0) {
      throw new Error(`issue-6472:\n  ${failures.join("\n  ")}`);
    }
  });
  console.log("issue-6472: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
