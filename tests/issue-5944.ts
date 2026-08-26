// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5944
//
// A /GoTo link with a /FitR destination must keep the annotation's /Rect as the
// clickable (and "Show Links") box. After #5828 the destination rectangle was
// written onto IPageDestination::rect and then copied onto the page element, so
// link boxes appeared at the destination, much larger than the annotation.
//
// The fixture has one small /FitR link and one /XYZ link on page 1. Both source
// boxes must stay ~30x15; only the destination of the /FitR link is the large
// rectangle. Following either link must still land on page 2 (the #5828 dest
// is not dropped).
//
// Run:  bun tests/issue-5944.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

function makePdf(): string {
  // page 1: two small link annots (30x15). dest page is 612x792 letter so the
  // /FitR region (200x140) is clearly larger than the source box.
  const annots =
    `<< /Type /Annot /Subtype /Link /Rect [50 50 80 65] /Border [0 0 1] ` +
    `/A << /S /GoTo /D [4 0 R /FitR 100 200 300 340] >> >>` +
    `<< /Type /Annot /Subtype /Link /Rect [150 50 180 65] /Border [0 0 1] ` +
    `/A << /S /GoTo /D [4 0 R /XYZ 72 720 null] >> >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Annots [${annots}] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
  ];
  return assemblePdf(objs);
}

function parseLinks(out: string): { page: number; src: number[]; dest: number[] }[] {
  const re = /^kind=\S+ page=(-?\d+) src=([^ ]+) dest=([^ ]+) value=.*$/gm;
  const links = [];
  for (const m of out.matchAll(re)) {
    links.push({
      page: +m[1]!,
      src: m[2]!.split(",").map(Number),
      dest: m[3]!.split(",").map(Number),
    });
  }
  return links;
}

function near(got: number, want: number, tol = 1.5): boolean {
  return Math.abs(got - want) <= tol;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5944.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const out = String(res[1] ?? res[0] ?? "");
    const links = parseLinks(out);
    if (links.length !== 2) {
      throw new Error(`expected 2 links, got ${links.length}:\n${out}`);
    }

    for (const link of links) {
      if (link.page !== 2) {
        throw new Error(`link should go to page 2, got ${link.page}:\n${out}`);
      }
      // source box is the 30x15 annotation, not the destination
      if (!near(link.src[2]!, 30) || !near(link.src[3]!, 15)) {
        throw new Error(`source link box should be 30x15, got ${link.src.join(",")}:\n${out}`);
      }
    }

    const fitR = links.find((l) => near(l.dest[2]!, 200) && near(l.dest[3]!, 140));
    const xyz = links.find((l) => l !== fitR);
    if (!fitR || !xyz) {
      throw new Error(`expected one /FitR dest of 200x140 and one /XYZ dest, got:\n${out}`);
    }
    // the /FitR dest must not have leaked onto the source box
    if (near(fitR.src[2]!, 200) || near(fitR.src[3]!, 140)) {
      throw new Error(`/FitR destination leaked onto the source link box:\n${out}`);
    }
    console.log(
      `  /FitR source ${fitR.src.slice(2).join("x")} dest ${fitR.dest.slice(2).join("x")}; /XYZ source ${xyz.src.slice(2).join("x")} ✓`,
    );
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
