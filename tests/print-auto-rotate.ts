// PrinterDefaults.AutoRotate = false prints a page wider than it is tall
// upright on portrait paper instead of turning it 90° to fill the sheet
// (discussion #6475: "I select Portrait and it still prints horizontal").
//
// The page has a red left half and a blue right one. In the printed PDF they
// sit side by side when the page is upright, one above the other when it was
// turned.
//
// Run: bun tests/print-auto-rotate.ts [--no-build]

import { rmSync, writeFileSync } from "node:fs";
import { requirePrintToPdf, runPrintToPdf, tempPrintOutput } from "./print-util.ts";
import { assemblePdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";

function makeWidePdf(): string {
  const content = "1 0 0 rg 0 0 300 400 re f 0 0 1 rg 300 0 300 400 re f";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 400] /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ]);
}

type Layout = "side-by-side" | "stacked";

// opens the printed PDF and tells how its red and blue halves are arranged
async function printedLayout(pdf: string): Promise<Layout> {
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await sleep(200);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error("print-auto-rotate: could not capture the canvas");
    }
    const sum = { red: { n: 0, x: 0, y: 0 }, blue: { n: 0, x: 0, y: 0 } };
    for (let i = 0; i < cap.data.length; i += 4) {
      const b = cap.data[i]!;
      const g = cap.data[i + 1]!;
      const r = cap.data[i + 2]!;
      const c = r > 180 && g < 80 && b < 80 ? sum.red : b > 180 && r < 80 && g < 80 ? sum.blue : null;
      if (!c) continue;
      const px = i / 4;
      c.n++;
      c.x += px % cap.w;
      c.y += Math.floor(px / cap.w);
    }
    if (sum.red.n < 1000 || sum.blue.n < 1000) {
      throw new Error(`print-auto-rotate: printed page lacks red or blue: ${JSON.stringify(sum)}`);
    }
    const dx = Math.abs(sum.red.x / sum.red.n - sum.blue.x / sum.blue.n);
    const dy = Math.abs(sum.red.y / sum.red.n - sum.blue.y / sum.blue.n);
    return dx > dy ? "side-by-side" : "stacked";
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function printWith(name: string, settings: string, want: Layout): Promise<void> {
  const input = tmpPath("print-auto-rotate.pdf");
  writeFileSync(input, makeWidePdf(), "latin1");
  const appdata = writeAppdata(`print-auto-rotate-${name}`, settings);
  const out = tempPrintOutput(`auto-rotate-${name}`);
  try {
    const res = runPrintToPdf(input, out, "portrait,fit", ["-appdata", appdata]);
    if (!res.ok) {
      throw new Error(`print-auto-rotate: ${name} print failed (exit ${res.exitCode})`);
    }
    const got = await printedLayout(out);
    if (got !== want) {
      throw new Error(`print-auto-rotate: ${name}: expected ${want}, got ${got}`);
    }
    console.log(`  ${name}: ${got}`);
  } finally {
    rmSync(out, { force: true });
  }
}

export async function testit(): Promise<void> {
  requirePrintToPdf();
  await printWith("default", "", "stacked");
  await printWith("AutoRotate=false", "PrinterDefaults [\n\tAutoRotate = false\n]\n", "side-by-side");
}

if (import.meta.main) {
  await runStandalone(testit);
}
