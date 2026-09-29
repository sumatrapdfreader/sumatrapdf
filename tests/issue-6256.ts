// Discussion #6256: saving annotations into a big PDF on a slow (cloud /
// network) drive froze the app for ~20 s. MuPDF's incremental save re-read the
// whole file to check it still holds the original bytes. Now the first change
// reads a big file into memory (in the background) and the save compares
// against that, so the drive only sees the appended bytes.
//
// Checks, for a PDF over the 32 MB in-memory limit:
//  - the first change loads the file into memory
//  - the save takes the in-memory path and only appends to the file
//  - the saved file is valid: the app reloads it without the deleted annotation
//
// Run: bun tests/issue-6256.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const FILLER_BYTES = 33 * 1024 * 1024;

function makePdf(): string {
  // an unreferenced stream that makes the file bigger than kMaxMemoryFileSize
  const filler = `<< /Length ${FILLER_BYTES} >>\nstream\n${"A".repeat(FILLER_BYTES)}\nendstream`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /BS << /W 2 >> >>",
    filler,
  ]);
}

type Markup = { annotations: number; square: { x: number; y: number } | null };

async function markup(client: ControlClient): Promise<Markup> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const count = /annotations=(\d+)/.exec(raw);
    if (res[0] === 0 && count) {
      const m = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
      const square = m ? { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) } : null;
      return { annotations: +count[1]!, square };
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6256: could not read annotations\n${raw}`);
    }
    await sleep(100);
  }
}

async function waitForLog(logPath: string, re: RegExp, what: string): Promise<string> {
  const deadline = Date.now() + 30_000;
  for (;;) {
    const log = existsSync(logPath) ? readFileSync(logPath, "utf8") : "";
    const m = re.exec(log);
    if (m) {
      return m[0];
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6256: ${what}; log:\n${log.slice(-3000)}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6256");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "big.pdf");
  const logPath = join(dir, "log.txt");
  const original = Buffer.from(makePdf(), "latin1");
  writeFileSync(pdf, original);

  const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    const before = await markup(client);
    if (before.annotations !== 1 || !before.square) {
      throw new Error(`issue-6256: expected one square, got ${before.annotations}`);
    }
    sendMessage(frame, WM_COMMAND, cmdId("CmdDeleteAnnotation"), packCoords(before.square.x, before.square.y));
    await waitForLog(logPath, /FileBytesLoaded: [^\n]*in memory: 1/, "the first change did not load the file");

    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    const saved = await waitForLog(logPath, /Saved annotations to [^\n]*/, "the save did not finish");
    if (!/from memory: 1/.test(saved)) {
      throw new Error(`issue-6256: the save re-read the file: ${saved}`);
    }

    // only appended: the original bytes are untouched
    const after = readFileSync(pdf);
    const grown = after.length - original.length;
    if (grown <= 0 || grown > 64 * 1024) {
      throw new Error(`issue-6256: file grew by ${grown} bytes, want a small increment`);
    }
    if (!after.subarray(0, original.length).equals(original)) {
      throw new Error("issue-6256: the save changed the original bytes");
    }

    // the app reloads the saved file: it must parse, without the annotation
    await waitForLog(logPath, /ReloadDocument: [^\n]*reloaded in/, "the saved file was not reloaded");
    const reloaded = await markup(client);
    if (reloaded.annotations !== 0) {
      throw new Error(`issue-6256: reloaded file has ${reloaded.annotations} annotation(s), want 0`);
    }
    console.log(`issue-6256: OK (${saved.trim()}, size ${statSync(pdf).size})`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
