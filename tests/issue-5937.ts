// #5937: the CAD/engineering-drawing enhancement darkens grays so thin lines
// stay readable when zoomed out, but it was also recoloring area fills, which
// turned 0.6 gray into 54 and 0.8 gray into 88 (near black) on a site plan.
// A fill that covers real area must keep its gray; a thin filled rectangle,
// which is how exporters often draw lines, must still be darkened.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const AREA_GRAY = 153; // 0.6 gray, as a big filled rectangle
const LINE_GRAY = 153; // the same gray, as a thin bar

const ZOOM_PERCENT = 100;

// The enhancement's strength comes from the ctm the display list replays with,
// which is page space -- so, like the CAD exports it targets, the fixture draws
// through a scaled `cm`. At 0.24 the blend is full. In page space the block is
// then 400x300pt (an area, must keep its gray) and the bar 2.4pt tall (a line
// an exporter drew as a fill, must still be darkened).
function makePdf(): string {
  const content = [
    "q 0.24 0 0 0.24 0 0 cm",
    "0.6 0.6 0.6 rg",
    "416 1666 1666 1250 re f", // area fill: must keep its gray
    "416 833 1666 10 re f", // line drawn as a thin fill: must be darkened
    "Q",
  ].join("\n");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << >> >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

function parseGrays(raw: string): Map<number, number> {
  const out = new Map<number, number>();
  for (const line of raw.split("\n")) {
    const m = line.trim().match(/^gray=(\d+) count=(\d+)$/);
    if (m) {
      out.set(Number(m[1]), Number(m[2]));
    }
  }
  return out;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5937.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const raw = await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      const res = await client.request(ControlCommand.TestCadEnhanceColors, [pdf, 1, ZOOM_PERCENT]);
      const exitCode = res[0] as number;
      const text = String(res[1] ?? "");
      if (exitCode !== 0) {
        throw new Error(`issue-5937: ${text.trim()}`);
      }
      return text;
    },
    [],
  );

  const grays = parseGrays(raw);
  // the block is 400x300 device px; allow for the antialiased edge
  const areaCount = grays.get(AREA_GRAY) ?? 0;
  if (areaCount < 100000) {
    const seen = [...grays.entries()].sort((a, b) => b[1] - a[1]).slice(0, 6);
    throw new Error(
      `issue-5937: area fill lost its gray ${AREA_GRAY}: count=${areaCount}, most common ${JSON.stringify(seen)}`,
    );
  }
  // the bar must have been pushed well below the area's gray
  const darkened = [...grays.entries()].filter(([v, n]) => v < LINE_GRAY - 40 && v > 0 && n >= 64);
  if (darkened.length === 0) {
    throw new Error(`issue-5937: thin fill was not darkened, grays: ${JSON.stringify([...grays.entries()])}`);
  }
  const darkest = Math.min(...darkened.map(([v]) => v));
  console.log(`issue-5937: area kept gray ${AREA_GRAY} (${areaCount}px), thin fill darkened to ${darkest}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
