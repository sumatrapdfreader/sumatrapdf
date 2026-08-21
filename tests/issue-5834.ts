// Test for issue #5834: "Allow Contents text input box in Annotation window to
// expand".
//
// The Contents box of the annotation editor was a fixed five lines, so a
// multi-line annotation stayed cut off however tall the window was. It now
// shares the spare vertical space with the list of annotations (see #3769), so
// making the window taller shows more of the annotation's text.
//
// The list is a VirtListBox (no Win32 ListBox HWND), so the test drives the
// editor through -dbg-control: select the annotation, resize the window, and
// check the Contents box grew.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// one page with a text annotation whose Contents needs many lines
function makePdf(): string {
  const contents = Array.from({ length: 12 }, (_, i) => `annotation line ${i + 1}`).join("\\n");
  const annot = `<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (tester) /Contents (${contents}) >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
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

async function measureContents(client: ControlClient, clientDy: number): Promise<{ contentsDy: number; raw: string }> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [clientDy, 1]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /contentsDy=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-5834: could not parse: ${raw}`);
      }
      return { contentsDy: parseInt(m[1]!, 10), raw };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5834: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5834: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5834.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client) => {
      const short = await measureContents(client, 560);
      const tall = await measureContents(client, 1000);
      if (tall.contentsDy <= short.contentsDy + 20) {
        throw new Error(
          `the Contents box does not use the extra space: ${short.contentsDy}px in a 560px tall window, ` +
            `${tall.contentsDy}px in a 1000px tall one (${short.raw} / ${tall.raw})`,
        );
      }
      console.log(`  Contents box grows with the window: ${short.contentsDy}px -> ${tall.contentsDy}px ✓`);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
