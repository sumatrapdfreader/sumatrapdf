// #6572: ReadingProgressBarDy draws a thin reading progress bar along the bottom of
// the document, filled as far as the current page is through it. 0 (the
// default) means no bar. Works for every document the canvas shows: PDF,
// image, comic, EPUB.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, makeTopBottomPdf, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, findCanvas } from "./win-automation.ts";
import { captureWindowPixels, sleep, waitForTopWindow } from "./winapi.ts";

type Rc = { x: number; y: number; dx: number; dy: number };
type Progress = { canvas: Rc; bar: Rc | null; progress: Rc | null; done: number; doneCol: string; restCol: string };

const kPages = 4;

function parseRc(s: string): Rc {
  const [x, y, dx, dy] = s.split(",").map(Number);
  return { x, y, dx, dy };
}

// "OK place=.. canvas=x,y,dx,dy bar=..\n...\nprogress=x,y,dx,dy done=N doneCol=rrggbb restCol=rrggbb"
// progress is in canvas coordinates
async function getProgress(client: ControlClient): Promise<Progress> {
  const [, raw] = await client.request(ControlCommand.TestPageInfoBar, []);
  const s = String(raw ?? "");
  const geo = /canvas=([\d,-]+) bar=([\w,-]+)/.exec(s);
  const p = /^progress=(\S+)(?: done=(\d+) doneCol=(\w+) restCol=(\w+))?$/m.exec(s);
  if (!s.startsWith("OK ") || !geo || !p) {
    throw new Error(`issue-6572: TestPageInfoBar: ${s}`);
  }
  return {
    canvas: parseRc(geo[1]),
    bar: geo[2] === "none" ? null : parseRc(geo[2]),
    progress: p[1] === "none" ? null : parseRc(p[1]),
    done: Number(p[2] ?? 0),
    doneCol: p[3] ?? "",
    restCol: p[4] ?? "",
  };
}

// commands are posted and painting follows: poll until ok(p)
async function waitProgress(client: ControlClient, what: string, ok: (p: Progress) => boolean): Promise<Progress> {
  let p = await getProgress(client);
  for (let i = 0; i < 50 && !ok(p); i++) {
    await sleep(100);
    p = await getProgress(client);
  }
  if (!ok(p)) {
    throw new Error(`issue-6572: ${what}: ${JSON.stringify(p)}`);
  }
  return p;
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

async function withSettings(
  tag: string,
  settings: string,
  file: string,
  fn: (client: ControlClient, pid: number) => Promise<void>,
): Promise<void> {
  const dir = tmpPath(`issue-6572-${tag}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `RestoreSession = false\n${settings}\n`);
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.waitForRenderIdle();
      await fn(client, proc.pid!);
    },
    ["-appdata", dir, file],
  );
}

// a thin bar across the whole canvas, ending `bottom` px above its bottom
function checkBar(p: Progress, what: string, bottom: number): Rc {
  const r = p.progress;
  const thin = r && r.dy >= 2 && r.dy <= 12;
  if (!r || !thin || r.x !== 0 || r.dx !== p.canvas.dx || r.y + r.dy !== p.canvas.dy - bottom) {
    throw new Error(`issue-6572: ${what}: bar misplaced: ${JSON.stringify(p)}`);
  }
  return r;
}

// the done part covers page/kPages of the bar
const doneFor = (p: Progress, page: number) => Math.abs(p.done - Math.round((p.canvas.dx * page) / kPages)) <= 1;

function pixelHex(px: { w: number; data: Uint8Array }, x: number, y: number): string {
  const i = (y * px.w + x) * 4;
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  return hex(px.data[i + 2]!) + hex(px.data[i + 1]!) + hex(px.data[i]!);
}

// the canvas shows the bar in the colors it reports
async function checkPainted(pid: number, p: Progress): Promise<void> {
  const frame = await waitForTopWindow(pid, FRAME_CLASS);
  const canvas = findCanvas(frame);
  if (!canvas || canvas === frame) {
    // ng draws the canvas in the frame: no window of its own to capture
    return;
  }
  const px = captureWindowPixels(canvas);
  if (!px) {
    throw new Error("issue-6572: no canvas pixels");
  }
  const r = p.progress!;
  const y = r.y + Math.floor(r.dy / 2);
  const done = pixelHex(px, Math.floor(p.done / 2), y);
  const rest = pixelHex(px, p.done + Math.floor((r.dx - p.done) / 2), y);
  if (done !== p.doneCol || rest !== p.restCol) {
    throw new Error(`issue-6572: painted done=${done} rest=${rest}, want ${p.doneCol} ${p.restCol}`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6572.pdf");
  writeFileSync(pdf, makeTopBottomPdf(kPages));

  // default: no bar
  await withSettings("off", "", pdf, async (client) => {
    await waitProgress(client, "default", (p) => p.progress === null);
  });

  await withSettings("on", "ReadingProgressBarDy = 3", pdf, async (client, pid) => {
    let p = await waitProgress(client, "page 1", (p) => p.progress !== null && doneFor(p, 1));
    checkBar(p, "page 1", 0);
    await checkPainted(pid, p);

    await invoke(client, "CmdGoToNextPage");
    await waitProgress(client, "page 2", (p) => doneFor(p, 2));
    await invoke(client, "CmdGoToLastPage");
    p = await waitProgress(client, "last page", (p) => doneFor(p, kPages));
    checkBar(p, "last page", 0);
  });

  // above the page info bar when it is drawn over the canvas
  await withSettings("overlay", "ReadingProgressBarDy = 3\nPageInfoLocation = bottom-overlay", pdf, async (client) => {
    const p = await waitProgress(client, "overlay", (p) => p.progress !== null && p.bar !== null);
    checkBar(p, "overlay", p.bar!.dy);
  });

  // every kind of document the canvas shows
  for (const name of ["issue-6225.jpg", "issue-1201.cbz", "issue-5846.epub"]) {
    await withSettings("format", "ReadingProgressBarDy = 3", join(import.meta.dir, name), async (client) => {
      const p = await waitProgress(client, name, (p) => p.progress !== null && p.done > 0);
      checkBar(p, name, 0);
    });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
