// Discussion #4276: a file-attachment annotation whose payload Sumatra can
// open (a PDF) loads it from memory into a new tab.
//
// Drive this through -dbg-control, not posted mouse clicks. SetCapture injects
// a WM_MOUSEMOVE at the real cursor, so a synthetic press is treated as a pan.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { getWindowText } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

function makeInnerPdf(): string {
  const stream = "BT /F1 24 Tf 72 720 Td (inner attachment) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ]);
}

function makePdfWithPdfAttachment(): string {
  const inner = makeInnerPdf();
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [200 400 400 600] /FS 5 0 R /Name /PushPin >>",
    "<< /Type /Filespec /F (inner.pdf) /UF (inner.pdf) /EF << /F 6 0 R >> >>",
    `<< /Type /EmbeddedFile /Length ${inner.length} /Params << /Size ${inner.length} >> >>\nstream\n${inner}\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-4276");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "outer.pdf");
  writeFileSync(pdf, makePdfWithPdfAttachment(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    const links = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const linkText = String(links[1] ?? "");
    if (!/kind=launchEmbedded/.test(linkText) || !/inner\.pdf/.test(linkText)) {
      throw new Error(`issue-4276: expected launchEmbedded inner.pdf, got:\n${linkText}`);
    }

    await pollUntil(
      async () => {
        const res = await client.request(ControlCommand.TestMarkupAnnots, ["open-embedded", 0, 0]);
        return { code: res[0], text: String(res[1] ?? "") };
      },
      (r) => r.code === 0 && /OK/.test(r.text),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (r) => `issue-4276: open-embedded failed: code=${r.code} ${r.text}`,
      },
    );

    const title = await pollUntil(
      () => getWindowText(frame),
      (t) => t.includes("inner.pdf"),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (t) => `issue-4276: expected tab inner.pdf, title='${t}'`,
      },
    );
    console.log(`issue-4276: opened ${title}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
