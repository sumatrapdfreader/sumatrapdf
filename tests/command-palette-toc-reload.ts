// The palette's TOC rows point at TocItems of the document's controller. An
// auto-reload while the palette is open frees them; the rows must follow the
// reloaded document, and Enter go where its TOC says.
//
// Run: bun tests/command-palette-toc-reload.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  killAndWait,
  launchControlled,
  ngKey,
  paletteSetText,
  paletteState,
  sendCommand,
  waitForPalette,
  type PaletteState,
} from "./win-automation.ts";
import { postMessage, sleep, VK_RETURN, WM_KEYDOWN } from "./winapi.ts";

const kTag = "command-palette-toc-reload";
const kPages = 3;
const kWaitMs = 10000;

// kPages pages, one bookmark `title` to page `page`
function makeTocPdf(title: string, page: number): string {
  const pageObj = (i: number) => 5 + i;
  const kids = Array.from({ length: kPages }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /Outlines 3 0 R >>",
    `<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`,
    "<< /Type /Outlines /First 4 0 R /Last 4 0 R /Count 1 >>",
    `<< /Title (${title}) /Parent 3 0 R /Dest [${pageObj(page - 1)} 0 R /Fit] >>`,
  ];
  for (let i = 0; i < kPages; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] >>");
  }
  return assemblePdf(objs);
}

async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
  const deadline = Date.now() + kWaitMs;
  for (;;) {
    const p = await paletteState(client);
    if (pred(p)) {
      return p;
    }
    if (Date.now() > deadline) {
      throw new Error(`${kTag}: ${what}; palette is ${JSON.stringify(p)}`);
    }
    await sleep(100);
  }
}

async function currentPage(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestCurrentTab, []))[1] ?? "");
  return Number(/\bpage=(\d+)/.exec(raw)?.[1] ?? 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath(kTag);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "toc.pdf");
  writeFileSync(pdf, makeTocPdf("Chapter", 3), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));
    const handles = await waitForPalette(client, frame);
    if (!handles.edit) {
      throw new Error(`${kTag}: the palette never opened`);
    }
    const query = "%Chapter";
    await paletteSetText(client, handles, query);
    await waitFor(client, `'${query}' did not produce one row`, (p) => p.open && p.items === 1 && p.sel === 0);

    // the file changes on disk: the bookmark is renamed and points elsewhere
    writeFileSync(pdf, makeTocPdf("Chapter Two", 2), "latin1");
    await waitFor(client, "the TOC row did not follow the reload", (p) => p.open && p.selText === "Chapter Two");

    if (USE_NG) {
      await ngKey(client, VK_RETURN);
    } else {
      postMessage(handles.edit, WM_KEYDOWN, VK_RETURN, 0);
    }
    const deadline = Date.now() + kWaitMs;
    let page = 0;
    while (Date.now() < deadline) {
      page = await currentPage(client);
      if (page === 2) {
        break;
      }
      await sleep(100);
    }
    if (page !== 2) {
      throw new Error(`${kTag}: Enter on the TOC row went to page ${page}, want 2`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log(`${kTag}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
