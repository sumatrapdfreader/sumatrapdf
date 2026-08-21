// Test for issue #3769: "annotation editor's list of annotations is not
// consistently sized".
//
// The list of annotations used to be a fixed number of lines - 5, or 14 once
// the window was taller than 1024 - so it stayed the same size no matter how
// big the window was (a 1000px tall editor left ~750px of empty space below the
// list), and a small difference in window height flipped it between two very
// different sizes. It now takes whatever vertical space the rest of the window
// doesn't need.
//
// The list is a VirtListBox (no Win32 ListBox HWND), so the test drives the
// editor through -dbg-control: resize it and check the list follows. The space
// below the list stays about the same while the list itself grows and shrinks
// with the window.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// a one-page PDF; the editor opens even with no annotations
function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
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

type Layout = { windowDy: number; listDy: number; contentsDy: number; gapBelow: number; raw: string };

async function measure(client: ControlClient, clientDy: number, selectItem = 0): Promise<Layout> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [clientDy, selectItem]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /windowDy=(\d+) listDy=(\d+) contentsDy=(\d+) gapBelow=(-?\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-3769: could not parse: ${raw}`);
      }
      return {
        windowDy: parseInt(m[1]!, 10),
        listDy: parseInt(m[2]!, 10),
        contentsDy: parseInt(m[3]!, 10),
        gapBelow: parseInt(m[4]!, 10),
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
      const tall = await measure(client, 1000);
      const short = await measure(client, 560);
      const tallAgain = await measure(client, 1000);

      const grew = tall.listDy - short.listDy;
      const windowGrew = tall.windowDy - short.windowDy;
      if (grew < windowGrew / 2) {
        throw new Error(
          `the list does not take the available space: window ${short.windowDy} -> ${tall.windowDy} ` +
            `but list only ${short.listDy} -> ${tall.listDy} (${tall.raw} / ${short.raw})`,
        );
      }
      if (tall.gapBelow > 200) {
        throw new Error(`${tall.gapBelow}px of unused space below the list in a ${tall.windowDy}px tall window`);
      }
      if (Math.abs(tallAgain.listDy - tall.listDy) > 4) {
        throw new Error(`same window height gave different list sizes: ${tall.listDy} then ${tallAgain.listDy}`);
      }
      console.log(
        `  annotation list follows the window: ${short.listDy}px at ${short.windowDy} -> ` +
          `${tall.listDy}px at ${tall.windowDy} (gap below ${tall.gapBelow}px) ✓`,
      );
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
