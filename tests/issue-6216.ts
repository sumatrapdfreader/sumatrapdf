// Save As on a PostScript document: a .pdf target gets the PDF Ghostscript
// produced, a .ps target a copy of the source (issue #6216). Skips without
// Ghostscript.
//
// Run: bun tests/issue-6216.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { findGhostscript, pdfToPs, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

async function saveAs(client: ControlClient, dst: string): Promise<void> {
  const res = await client.request(ControlCommand.TestSaveFileAs, [dst]);
  if (res[0] !== 0) {
    throw new Error(`issue-6216: save as ${dst}: ${String(res[1] ?? "").trim()}`);
  }
  if (!existsSync(dst)) {
    throw new Error(`issue-6216: ${dst} was not written`);
  }
}

export async function testit(): Promise<void> {
  const gs = findGhostscript();
  if (!gs) {
    console.log("issue-6216: skipped, Ghostscript is not installed (get it from ghostscript.com)");
    return;
  }

  const dir = tmpPath("issue-6216");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const ps = join(dir, "doc.ps");
  await pdfToPs(gs, join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), ps);
  const psBytes = readFileSync(ps);

  const { proc, client } = await launchControlled([ps]);
  try {
    await client.waitForRenderIdle(30000);

    const pdf = join(dir, "saved.pdf");
    await saveAs(client, pdf);
    const pdfBytes = readFileSync(pdf);
    if (!pdfBytes.subarray(0, 5).equals(Buffer.from("%PDF-"))) {
      throw new Error(`issue-6216: ${pdf} is not a PDF (${pdfBytes.length} bytes)`);
    }

    const psCopy = join(dir, "copy.ps");
    await saveAs(client, psCopy);
    if (!readFileSync(psCopy).equals(psBytes)) {
      throw new Error(`issue-6216: ${psCopy} differs from the source`);
    }
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  await proc.exited;
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
