// CmdToggleLineWeights, discussion #6567: with line weights off,
// every stroke is drawn one pixel wide on screen, like Acrobat's View >
// Display > Line Weights. Exported (and printed) pages keep the real widths.
//
// Run: bun tests/issue-6567.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

// one 160pt long horizontal line, 20pt wide: ~3200 dark pixels at 100%
const LINE_DX = 160;
const LINE_WIDTH = 20;
const THICK_MIN = LINE_DX * LINE_WIDTH * 0.8;
// a hairline is a pixel or two tall
const THIN_MAX = LINE_DX * 3;

function buildPdf(): Buffer {
  const stream = `0 g 0 G ${LINE_WIDTH} w 20 100 m ${20 + LINE_DX} 100 l S`;
  const objs = [
    "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
    "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
    "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R >>\nendobj\n",
    `4 0 obj\n<< /Length ${stream.length} >>\nstream\n${stream}\nendstream\nendobj\n`,
  ];
  const header = "%PDF-1.4\n";
  const offsets: number[] = [];
  let pos = header.length;
  for (const o of objs) {
    offsets.push(pos);
    pos += o.length;
  }
  let xref = `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    xref += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  const trailer = `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF`;
  return Buffer.from(header + objs.join("") + xref + trailer, "latin1");
}

const VIEW = 1;
const EXPORT = 0;

async function darkPixels(client: ControlClient, pdf: string, target: number): Promise<number> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [pdf, 1, 100, target]);
  const raw = String(res[1] ?? "");
  const m = /nonwhite=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6567: render: ${raw.trim()}`);
  }
  return Number(m[1]);
}

// the toggle is posted: poll until the view render agrees
async function waitView(client: ControlClient, pdf: string, ok: (n: number) => boolean, what: string) {
  const deadline = Date.now() + 8000;
  let n = await darkPixels(client, pdf, VIEW);
  while (!ok(n) && Date.now() < deadline) {
    await sleep(50);
    n = await darkPixels(client, pdf, VIEW);
  }
  if (!ok(n)) {
    throw new Error(`issue-6567: ${what}: ${n} dark pixels`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6567.pdf");
  writeFileSync(pdf, buildPdf());
  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await waitView(client, pdf, (n) => n >= THICK_MIN, "line weights on: line should be thick");

    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleLineWeights"]);
    await waitView(client, pdf, (n) => n > 0 && n <= THIN_MAX, "line weights off: line should be a hairline");
    const exported = await darkPixels(client, pdf, EXPORT);
    if (exported < THICK_MIN) {
      throw new Error(`issue-6567: export must keep the line width, got ${exported} dark pixels`);
    }

    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleLineWeights"]);
    await waitView(client, pdf, (n) => n >= THICK_MIN, "line weights back on: line should be thick");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
