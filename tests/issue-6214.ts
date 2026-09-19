// Issue #6214: Copy Image / Save Image handed out the raw image samples, so
// an image a PDF draws flipped or rotated (via its cm matrix) came out in its
// stored orientation, not the way it looks on the page.
//
// Builds a PDF with a 3x2 image (red green yellow / blue white black) drawn
// normally, vertically flipped, and rotated 90 degrees, and checks the
// extracted bitmap's corners match what the page shows.
//
// Run: bun tests/issue-6214.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const RED = "ff0000";
const GREEN = "00ff00";
const YELLOW = "ffff00";
const BLUE = "0000ff";
const WHITE = "ffffff";
const BLACK = "000000";

type Corners = { w: number; h: number; tl: string; tr: string; bl: string; br: string };

// one page per way of drawing the image: the cm matrix before "/Im1 Do"
const PAGES: { name: string; cm: string; want: Corners }[] = [
  { name: "normal", cm: "100 0 0 100 50 50", want: { w: 3, h: 2, tl: RED, tr: YELLOW, bl: BLUE, br: BLACK } },
  { name: "vflip", cm: "100 0 0 -100 50 150", want: { w: 3, h: 2, tl: BLUE, tr: BLACK, bl: RED, br: YELLOW } },
  { name: "hflip", cm: "-100 0 0 100 150 50", want: { w: 3, h: 2, tl: YELLOW, tr: RED, bl: BLACK, br: BLUE } },
  { name: "rot90ccw", cm: "0 100 -100 0 150 50", want: { w: 2, h: 3, tl: YELLOW, tr: BLACK, bl: RED, br: BLUE } },
];

function makePdf(): Buffer {
  const img = Buffer.from([255, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0]);
  const objs: Buffer[] = [];
  const add = (b: Buffer | string): number => {
    objs.push(typeof b === "string" ? Buffer.from(b, "latin1") : b);
    return objs.length;
  };
  const pagesNo = 2;
  add("<< /Type /Catalog /Pages 2 0 R >>");
  add(""); // placeholder for /Pages
  const imgNo = add(
    Buffer.concat([
      Buffer.from(
        `<< /Type /XObject /Subtype /Image /Width 3 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length ${img.length} >>\nstream\n`,
        "latin1",
      ),
      img,
      Buffer.from("\nendstream", "latin1"),
    ]),
  );
  const pageNos: number[] = [];
  for (const p of PAGES) {
    const content = `q ${p.cm} cm /Im1 Do Q`;
    const cNo = add(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
    const pNo = add(
      `<< /Type /Page /Parent ${pagesNo} 0 R /MediaBox [0 0 200 200] /Resources << /XObject << /Im1 ${imgNo} 0 R >> >> /Contents ${cNo} 0 R >>`,
    );
    pageNos.push(pNo);
  }
  objs[pagesNo - 1] = Buffer.from(
    `<< /Type /Pages /Kids [${pageNos.map((n) => `${n} 0 R`).join(" ")}] /Count ${pageNos.length} >>`,
    "latin1",
  );

  const parts: Buffer[] = [Buffer.from("%PDF-1.4\n", "latin1")];
  const offsets: number[] = [];
  let pos = parts[0]!.length;
  objs.forEach((o, i) => {
    offsets.push(pos);
    const b = Buffer.concat([Buffer.from(`${i + 1} 0 obj\n`, "latin1"), o, Buffer.from("\nendobj\n", "latin1")]);
    parts.push(b);
    pos += b.length;
  });
  const xref =
    `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n` +
    offsets.map((o) => `${String(o).padStart(10, "0")} 00000 n \n`).join("") +
    `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`;
  parts.push(Buffer.from(xref, "latin1"));
  return Buffer.concat(parts);
}

function parseCorners(raw: string): Corners {
  const v: Record<string, string> = {};
  for (const part of raw.trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) {
      v[part.slice(0, eq)] = part.slice(eq + 1);
    }
  }
  return { w: Number(v.w), h: Number(v.h), tl: v.tl ?? "", tr: v.tr ?? "", bl: v.bl ?? "", br: v.br ?? "" };
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6214.pdf");
  writeFileSync(pdfPath, makePdf());

  await withControlledSumatra(EXE, async (client) => {
    for (let i = 0; i < PAGES.length; i++) {
      const p = PAGES[i]!;
      const res = await client.request(ControlCommand.TestImageOrientation, [pdfPath, i + 1]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-6214: ${p.name}: ${raw.trim()}`);
      }
      const got = parseCorners(raw);
      for (const k of ["w", "h", "tl", "tr", "bl", "br"] as const) {
        if (got[k] !== p.want[k]) {
          throw new Error(`issue-6214: ${p.name}: ${k}=${got[k]}, want ${p.want[k]}\n  ${raw.trim()}`);
        }
      }
      console.log(`  ${p.name}: ${raw.trim()} ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
