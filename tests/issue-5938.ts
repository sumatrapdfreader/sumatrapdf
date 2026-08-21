// Test for https://github.com/sumatrapdfreader/sumatrapdf/discussions/5938
//
// Feature: an option to keep the current zoom when following a "Go to
// Destination" action -- clicking a bookmark or a link inside the document.
// Adobe Reader and Foxit both have it ("forbid the change of the current Zoom
// factor during execution of 'Go to Destination' actions"); since 3.7 SumatraPDF
// applies the Adobe /Fit and /XYZ zoom modes (#5828), so there has to be a way
// to say "don't".
//
// Builds a PDF whose outline has two destinations on page 2, one with an
// explicit /XYZ zoom of 150% and one with /Fit (fit the page), zooms the reader
// to 100%, follows each destination, and checks the zoom afterwards:
//
//   IgnoreDestinationZoom = false (default) -> the destination's zoom wins
//   IgnoreDestinationZoom = true            -> the reader stays at 100%
//
// Either way the destination still decides the page, which is checked too --
// the option must not turn bookmarks into no-ops.
//
// Run:  bun tests/issue-5938.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// zoom is reported as SumatraPDF's "virtual" zoom: a percentage when it is a
// number, or one of the negative sentinels for the fit modes (Settings.h)
const kZoomFitPage = -1;

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

// a 2 page PDF whose outline has: /XYZ with zoom 1.5, and /Fit
function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Outlines /First 6 0 R /Last 7 0 R /Count 2 >>`,
    `<< /Title (XYZ zoom 1.5) /Parent 5 0 R /Next 7 0 R /Dest [4 0 R /XYZ 100 700 1.5] >>`,
    `<< /Title (Fit page) /Parent 5 0 R /Prev 6 0 R /Dest [4 0 R /Fit] >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

type Nav = { destZoom: number; page: number; landed: number; zoomBefore: number; zoomAfter: number; raw: string };

// zoom to startZoomPerc, then follow the destNo-th outline destination
async function followDest(client: ControlClient, destNo: number, startZoomPerc: number): Promise<Nav> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDestZoomNav, [destNo, startZoomPerc]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /destZoom=([-\d.eE+]+) page=(\d+) landed=(\d+) zoomBefore=([-\d.eE+]+) zoomAfter=([-\d.eE+]+)/.exec(
        raw,
      );
      if (!m) {
        throw new Error(`issue-5938: could not parse: ${raw}`);
      }
      return {
        destZoom: parseFloat(m[1]!),
        page: parseInt(m[2]!),
        landed: parseInt(m[3]!),
        zoomBefore: parseFloat(m[4]!),
        zoomAfter: parseFloat(m[5]!),
        raw,
      };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5938: TestDestZoomNav failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5938: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

async function run(pdf: string, ignoreDestinationZoom: boolean): Promise<Nav[]> {
  const dir = tmpPath(`issue-5938-${ignoreDestinationZoom ? "ignore" : "apply"}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `${SETTINGS}IgnoreDestinationZoom = ${ignoreDestinationZoom ? "true" : "false"}\n`,
  );
  return withControlledSumatra(
    EXE,
    async (client) => [await followDest(client, 1, 100), await followDest(client, 2, 100)],
    ["-appdata", dir, pdf],
  );
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5938.pdf");
  writeFileSync(pdf, makePdf());

  const applied = await run(pdf, false);
  const ignored = await run(pdf, true);

  const problems: string[] = [];
  const names = ["/XYZ 100 700 1.5", "/Fit"];
  // what the destination asks for, in the units zoomAfter is reported in
  const wantApplied = [150, kZoomFitPage];

  for (let i = 0; i < names.length; i++) {
    const a = applied[i]!;
    const g = ignored[i]!;
    console.log(`  dest ${i + 1} (${names[i]}):`);
    console.log(`    IgnoreDestinationZoom = false -> ${a.raw}`);
    console.log(`    IgnoreDestinationZoom = true  -> ${g.raw}`);

    // the destination decides the page whatever the option says
    for (const [what, r] of [
      ["applying", a],
      ["ignoring", g],
    ] as const) {
      if (r.landed !== r.page) {
        problems.push(
          `dest ${i + 1} (${names[i]}), ${what}: landed on page ${r.landed}, destination is page ${r.page}`,
        );
      }
    }

    // off: the document's zoom wins
    if (Math.abs(a.zoomAfter - wantApplied[i]!) > 0.01) {
      problems.push(
        `dest ${i + 1} (${names[i]}): with IgnoreDestinationZoom = false the zoom should have become ` +
          `${wantApplied[i]}, is ${a.zoomAfter}`,
      );
    }
    // on: the zoom the reader was at is kept
    if (Math.abs(g.zoomAfter - g.zoomBefore) > 0.01) {
      problems.push(
        `dest ${i + 1} (${names[i]}): with IgnoreDestinationZoom = true the zoom should have stayed at ` +
          `${g.zoomBefore}, became ${g.zoomAfter}`,
      );
    }
  }

  rmSync(pdf, { force: true });
  if (problems.length > 0) {
    throw new Error(`issue-5938:\n  ${problems.join("\n  ")}`);
  }
  console.log("✅ IgnoreDestinationZoom keeps the current zoom; without it the destination's zoom is applied");
}

if (import.meta.main) {
  await runStandalone(testit);
}
