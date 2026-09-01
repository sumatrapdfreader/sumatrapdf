// Discussion #4276: double-click a file-attachment annotation whose payload
// Sumatra can open (a PDF) loads it from memory into a new tab.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import {
  getWindowText,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  clientToScreen,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

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

function parseAttachmentScreen(raw: string): { x: number; y: number; dx: number; dy: number } | null {
  const m = /type=FileAttachment page=\d+ rect=[^ ]+ screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!m) {
    return null;
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function doubleClickAt(canvas: number, x: number, y: number): void {
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  const lp = packCoords(x, y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, lp);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  sendMessage(canvas, WM_LBUTTONUP, 0, lp);
  sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
  sendMessage(canvas, WM_LBUTTONUP, 0, lp);
}

async function attachmentScreen(client: ControlClient): Promise<{ x: number; y: number; dx: number; dy: number }> {
  return pollUntil(
    async () => {
      const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
      return parseAttachmentScreen(raw);
    },
    (r) => r !== null && r.dx > 0 && r.dy > 0,
    { timeoutMs: 8000, error: "issue-4276: file attachment not on page" },
  ) as Promise<{ x: number; y: number; dx: number; dy: number }>;
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

    const screen = await attachmentScreen(client);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-4276: canvas missing");
    }
    doubleClickAt(canvas, screen.x + Math.floor(screen.dx / 2), screen.y + Math.floor(screen.dy / 2));

    const title = await pollUntil(
      () => getWindowText(frame),
      (t) => t.includes("inner.pdf"),
      { timeoutMs: 8000, error: (t) => `issue-4276: expected tab inner.pdf, title='${t}'` },
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
