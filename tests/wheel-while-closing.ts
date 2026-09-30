// A mouse wheel that reaches a window while it is being closed: the frame
// forwards it to the canvas, whose DefWindowProc passed it back to the frame,
// recursing until the stack overflowed.
//
// Run: bun tests/wheel-while-closing.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { makePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const dir = tmpPath("wheel-while-closing");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(2, 601, 0, 0), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const res = await client.request(ControlCommand.TestWheelWhileClosing, []);
    if (res[0] !== 0 || String(res[1]) !== "OK") {
      throw new Error(`wheel-while-closing: ${String(res[1])}`);
    }
    // still alive: it answers another request
    await client.waitForRenderIdle();
    console.log("wheel-while-closing: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
