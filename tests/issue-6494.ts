// Test for issue #6494: PageInfoLocation = bottom / bottom-overlay shows Toggle
// Page Info as a bar along the bottom of the window, with more about the file
// and the page.
//
// For each mode: where the bar is relative to the canvas, that only "bottom"
// takes a row from the canvas, what the bar says about a PDF as pages turn, and
// that Toggle Page Info hides it. Then the per-format details for an image, a
// comic, an EPUB and a DjVu.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, makeTopBottomPdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

type Rc = { x: number; y: number; dx: number; dy: number };
type Bar = { place: string; canvas: Rc; bar: Rc | null; left: string; right: string };

function parseRc(s: string): Rc {
  const [x, y, dx, dy] = s.split(",").map(Number);
  return { x, y, dx, dy };
}

// "OK place=row canvas=1,71,925,762 bar=1,833,942,22\nleft=...\nright=..."
async function getBar(client: ControlClient): Promise<Bar> {
  const [, raw] = await client.request(ControlCommand.TestPageInfoBar, []);
  const s = String(raw ?? "");
  const m = /place=(\w+) canvas=([\d,-]+) bar=([\w,-]+)/.exec(s);
  if (!s.startsWith("OK ") || !m) {
    throw new Error(`issue-6494: TestPageInfoBar: ${s}`);
  }
  const line = (k: string) => (s.split("\n").find((l) => l.startsWith(k + "=")) ?? "").slice(k.length + 1);
  return {
    place: m[1],
    canvas: parseRc(m[2]),
    bar: m[3] === "none" ? null : parseRc(m[3]),
    left: line("left"),
    right: line("right"),
  };
}

// commands are posted and layout follows: poll until ok(bar)
async function waitBar(client: ControlClient, what: string, ok: (b: Bar) => boolean): Promise<Bar> {
  let b = await getBar(client);
  for (let i = 0; i < 50 && !ok(b); i++) {
    await sleep(100);
    b = await getBar(client);
  }
  if (!ok(b)) {
    throw new Error(`issue-6494: ${what}: ${JSON.stringify(b)}`);
  }
  return b;
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

async function withMode(mode: string, file: string, fn: (client: ControlClient) => Promise<void>): Promise<void> {
  const dir = tmpPath(`issue-6494-${mode}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `RestoreSession = false\nPageInfoLocation = ${mode}\n`);
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      await fn(client);
    },
    ["-appdata", dir, file],
  );
}

const bottomOf = (r: Rc) => r.y + r.dy;

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6494.pdf");
  writeFileSync(pdf, makeTopBottomPdf(2));

  let canvasDy = 0;
  await withMode("notification", pdf, async (client) => {
    const b = await waitBar(client, "notification", (b) => b.place === "none" && b.bar === null);
    canvasDy = b.canvas.dy;
  });

  await withMode("bottom", pdf, async (client) => {
    // a row of its own, under the canvas, which is that much shorter
    let b = await waitBar(client, "bottom", (b) => b.place === "row" && b.bar !== null);
    const bar = b.bar!;
    if (bar.y < bottomOf(b.canvas) - 1 || b.canvas.dy > canvasDy - bar.dy + 2) {
      throw new Error(`issue-6494: bottom: the bar isn't a row under the canvas: ${JSON.stringify(b)}`);
    }
    const wantLeft = ["issue-6494.pdf", "PDF 1.", "2 pages"];
    const wantRight = ["Page 1 / 2 (50%)", "(Letter)", "Zoom"];
    for (const w of wantLeft) {
      if (!b.left.includes(w)) {
        throw new Error(`issue-6494: left "${b.left}" lacks "${w}"`);
      }
    }
    for (const w of wantRight) {
      if (!b.right.includes(w)) {
        throw new Error(`issue-6494: right "${b.right}" lacks "${w}"`);
      }
    }
    await invoke(client, "CmdGoToNextPage");
    await waitBar(client, "next page", (b) => b.right.includes("Page 2 / 2 (100%)"));

    // Toggle Page Info hides it and gives the row back to the canvas
    await invoke(client, "CmdTogglePageInfo");
    b = await waitBar(client, "hidden", (b) => b.place === "none" && b.canvas.dy >= canvasDy - 1);
    await invoke(client, "CmdTogglePageInfo");
    await waitBar(client, "shown again", (b) => b.place === "row");
  });

  await withMode("bottom-overlay", pdf, async (client) => {
    // over the bottom of the canvas, which keeps its size
    const b = await waitBar(client, "bottom-overlay", (b) => b.place === "overlay" && b.bar !== null);
    const bar = b.bar!;
    const inside = bar.y >= b.canvas.y && bottomOf(bar) <= bottomOf(b.canvas) + 1;
    if (!inside || Math.abs(b.canvas.dy - canvasDy) > 1) {
      throw new Error(`issue-6494: bottom-overlay: the bar isn't over the canvas: ${JSON.stringify(b)}`);
    }
  });

  // what the bar says about other formats
  const formats: [string, string, string][] = [
    ["issue-6225.jpg", "right", "400 × 2000 px"],
    ["issue-1201.cbz", "right", "001.png"],
    ["issue-5846.epub", "left", "EPUB"],
    ["issue-6312.djvu", "right", "600 DPI"],
  ];
  for (const [name, side, want] of formats) {
    await withMode("bottom", join(import.meta.dir, name), async (client) => {
      await waitBar(client, name, (b) => (side === "left" ? b.left : b.right).includes(want));
    });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
