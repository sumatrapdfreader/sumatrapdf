// Maximized, the frame's non-client area hangs over the monitor's edges: onto a
// neighbor monitor and under the taskbar. Our WM_NCPAINT painted it, which showed
// as a strip on the other monitor. It must paint nothing then, while a restored
// window still gets its strips painted (issue #5851).
//
// The painting itself can't be seen here: on a single monitor the overhang is off
// the screen and GDI clips it away. So this checks what WM_NCPAINT would fill.
//
// Run: bun tests/issue-6259.ts [--no-build]

import { ControlClient, ControlCommand } from "./control.ts";
import { makeOnePagePdf, runStandalone, tmpPath } from "./util.ts";
import { postMessage, sleep, WM_SYSCOMMAND } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { writeFileSync } from "node:fs";

const SC_MAXIMIZE = 0xf030;

type Strips = { zoomed: boolean; count: number; raw: string };

async function ncStrips(client: ControlClient): Promise<Strips> {
  const res = await client.request(ControlCommand.TestFrameNcStrips, []);
  const raw = String(res[1] ?? "");
  const m = /zoomed=(\d) strips=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6259: TestFrameNcStrips: ${raw}`);
  }
  return { zoomed: m[1] === "1", count: +m[2]!, raw };
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6259.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    let s = await ncStrips(client);
    if (s.zoomed || s.count === 0) {
      throw new Error(`issue-6259: a restored window has non-client strips to paint: ${s.raw}`);
    }

    postMessage(frame, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    const deadline = Date.now() + 5000;
    while (!(s = await ncStrips(client)).zoomed) {
      if (Date.now() > deadline) {
        throw new Error(`issue-6259: the window didn't maximize: ${s.raw}`);
      }
      await sleep(50);
    }
    if (s.count !== 0) {
      throw new Error(`issue-6259: maximized, the frame must not paint its non-client area: ${s.raw}`);
    }
    console.log("issue-6259: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
