// Test for issue #3769: annotation editor list height.
//
// The list is as tall as the annotations, at least 6 and capped at 12 rows.
// Contents is a fixed 6 lines. Extra window height is empty space, so
// resizing or moving between annotations (which shows different per-type
// fields) does not change the list or Contents height.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const kAnnotCount = 20;

function makePdf(): string {
  const annots: string[] = [];
  annots.push(`<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (t1) /Contents (note one) >>`);
  annots.push(
    `<< /Type /Annot /Subtype /FreeText /Rect [80 650 220 690] /Contents (free text) /DA (/Helv 12 Tf 0 0 0 rg) >>`,
  );
  for (let i = 2; i < kAnnotCount; i++) {
    const y = 640 - i * 12;
    annots.push(`<< /Type /Annot /Subtype /Highlight /Rect [50 ${y} 200 ${y + 10}] /Contents (hl ${i}) >>`);
  }
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
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

type Layout = { windowDy: number; listDy: number; contentsDy: number; n: number; raw: string };

async function measure(client: ControlClient, clientDy: number, selectItem = 0): Promise<Layout> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [clientDy, selectItem]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /windowDy=(\d+) listDy=(\d+) contentsDy=(\d+).* n=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-3769: could not parse: ${raw}`);
      }
      return {
        windowDy: parseInt(m[1]!, 10),
        listDy: parseInt(m[2]!, 10),
        contentsDy: parseInt(m[3]!, 10),
        n: parseInt(m[4]!, 10),
        raw,
      };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-3769: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-3769: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3769.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client) => {
      const short = await measure(client, 560, 0);
      const tall = await measure(client, 1000, 0);
      if (short.n !== kAnnotCount) {
        throw new Error(`issue-3769: expected ${kAnnotCount} annotations, got n=${short.n} (${short.raw})`);
      }
      if (Math.abs(tall.listDy - short.listDy) > 8) {
        throw new Error(
          `list height should not follow the window: ${short.listDy}px at ${short.windowDy} vs ` +
            `${tall.listDy}px at ${tall.windowDy} (${short.raw} / ${tall.raw})`,
        );
      }

      const text = await measure(client, 720, 1);
      const freeText = await measure(client, 720, 2);
      if (Math.abs(text.listDy - freeText.listDy) > 4) {
        throw new Error(
          `list height changed when navigating: Text ${text.listDy}px vs FreeText ${freeText.listDy}px ` +
            `(${text.raw} / ${freeText.raw})`,
        );
      }
      if (Math.abs(text.contentsDy - freeText.contentsDy) > 8) {
        throw new Error(
          `Contents height changed when navigating: Text ${text.contentsDy}px vs FreeText ${freeText.contentsDy}px ` +
            `(${text.raw} / ${freeText.raw})`,
        );
      }
      console.log(
        `  annotation list stays put: ${short.listDy}px at ${short.windowDy}/${tall.windowDy}, ` +
          `Text/FreeText ${text.listDy}/${freeText.listDy}, Contents ${text.contentsDy}/${freeText.contentsDy} ✓`,
      );
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
