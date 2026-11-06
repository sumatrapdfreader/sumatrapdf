// The toolbar wraps its buttons onto a second row when the window is too
// narrow for one, instead of cutting off the buttons on the right. The Edit
// PDF row goes below the wrapped rows.
//
// Run: bun tests/toolbar-wrap.ts [--no-build]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowToPng, findChildWindow, sleep } from "./winapi.ts";

// wide enough for one row, and too narrow for it
const kWideDx = 1400;
const kNarrowDx = 420;

async function toolbarDy(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const m = /item name=toolbar visible=1 rect=-?\d+,-?\d+,\d+,(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`toolbar-wrap: no toolbar in the layout: ${raw}`);
  }
  return +m[1]!;
}

// toolbar height with and without the Edit PDF row
async function measure(dx: number, png: string): Promise<{ dy: number; editDy: number }> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-window-pos", `${dx}x700@0x0`, pdf]);
  try {
    await client.waitForRenderIdle();
    await sleep(200);
    const dy = await toolbarDy(client);
    const hwnd = findChildWindow(frame, "SUMATRA_VIRT_TOOLBAR");
    if (hwnd) {
      captureWindowToPng(hwnd, tmpPath(png));
    }
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.waitForRenderIdle();
    await sleep(200);
    const editDy = await toolbarDy(client);
    return { dy, editDy };
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const wide = await measure(kWideDx, "toolbar-wrap-wide.png");
  const narrow = await measure(kNarrowDx, "toolbar-wrap-narrow.png");
  console.log(`  toolbar height: ${JSON.stringify(wide)} at ${kWideDx}px, ${JSON.stringify(narrow)} at ${kNarrowDx}px`);
  const row = wide.dy;
  if (narrow.dy < row * 2) {
    throw new Error(`toolbar-wrap: a narrow window should wrap the toolbar onto more rows (${row} -> ${narrow.dy})`);
  }
  if (wide.editDy !== row * 2 || narrow.editDy !== narrow.dy + row) {
    throw new Error(
      `toolbar-wrap: the Edit PDF row should add one row: wide ${JSON.stringify(wide)}, narrow ${JSON.stringify(narrow)}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
