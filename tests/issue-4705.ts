// Opening a PDF from a OneNote-style cache path must drop the original file
// so the host can exclusive-open / delete it (issue #4705).
//
// Run: bun tests/issue-4705.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync, existsSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, tmpPath } from "./util.ts";
import { launchControlled, killAndWait, takeStderr } from "./win-automation.ts";
import { tryOpenExclusive, tryDeleteFile, getWindowText } from "./winapi.ts";

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const stream = "BT /F1 24 Tf 72 720 Td (issue 4705) Tj ET";
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= 5; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 6\n0000000000 65535 f \n`;
  for (let n = 1; n <= 5; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-4705");
  rmSync(dir, { recursive: true, force: true });
  const cacheDir = join(dir, "OneNote", "16.0", "cache");
  mkdirSync(cacheDir, { recursive: true });
  const pdf = join(cacheDir, "attachment.pdf");
  writeFileSync(pdf, makePdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const title = getWindowText(frame);
    if (!title.includes("attachment.pdf")) {
      throw new Error(`issue-4705: expected original name in title, got '${title}'`);
    }

    const exclusive = tryOpenExclusive(pdf);
    if (!exclusive.ok) {
      throw new Error(`issue-4705: original still locked for exclusive write (error ${exclusive.error})`);
    }
    const del = tryDeleteFile(pdf);
    if (!del.ok) {
      throw new Error(`issue-4705: cannot delete original while Sumatra has it open (error ${del.error})`);
    }
    if (existsSync(pdf)) {
      throw new Error("issue-4705: original still exists after DeleteFile");
    }
    // FileWatcher used to hold the host cache directory; OneNote cannot replace
    // that folder while a handle is open. The whole extract tree must go.
    try {
      rmSync(join(dir, "OneNote"), { recursive: true, force: false });
    } catch (e) {
      throw new Error(`issue-4705: cannot remove OneNote cache dir while Sumatra is open: ${String(e)}`);
    }
    await client.waitForRenderIdle();
    if (!getWindowText(frame).includes("attachment.pdf")) {
      throw new Error("issue-4705: document gone after host cache was deleted");
    }
  } catch (e) {
    try {
      await client.quit();
    } catch {
      /* already gone */
    }
    await killAndWait(proc);
    const err = await takeStderr(proc);
    if (err) {
      console.error(err);
    }
    throw e;
  } finally {
    try {
      await client.quit();
    } catch {
      /* already gone */
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
