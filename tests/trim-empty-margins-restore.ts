// Opening a document whose remembered state has TrimEmptyMargins = true fired
// a debug report and terminated the app: ReplaceDocumentInCurrentTab() calls
// SetTrimEmptyMargins() before SetInitialViewSettings(), so the relayout it
// does ran on a DisplayModel with no pagesInfo yet.
//
// Setting the flag is enough that early: the initial layout honours it. The
// trim has to actually happen, so the test also checks that the page is laid
// out differently than with the margins left alone.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { getScrollInfo } from "./winapi.ts";

const PAGE_COUNT = 2;

// letter-size pages with one small block of content in the middle, so trimming
// the empty margins shrinks the layout to a fraction of its untrimmed height
function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  const kids: number[] = [];
  let objNum = 3;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const content = "0 0 0 rg 250 380 110 40 re f";
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents ${contentNum} 0 R /Resources << >> >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${PAGE_COUNT} >>`;
  const maxN = objNum - 1;
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= maxN; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function writeSettings(appdata: string, pdf: string, trim: boolean): void {
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "RememberStatePerDocument = true",
      "FileStates [",
      "\t[",
      `\t\tFilePath = ${pdf}`,
      `\t\tTrimEmptyMargins = ${trim ? "true" : "false"}`,
      "\t\tPageNo = 1",
      "\t]",
      "]",
      "",
    ].join("\n"),
  );
}

// scroll extent, and the first page's height/width
async function pageLayout(dir: string, pdf: string, trim: boolean): Promise<{ extent: number; aspect: number }> {
  const appdata = join(dir, `appdata-${trim ? "on" : "off"}`);
  rmSync(appdata, { recursive: true, force: true });
  writeSettings(appdata, pdf, trim);
  let launched;
  try {
    launched = await launchControlled(["-appdata", appdata, pdf]);
  } catch (e) {
    // a debug report kills the app before the control pipe is up
    throw new Error(`trim-empty-margins-restore: the app died loading with TrimEmptyMargins=${trim}: ${e}`);
  }
  const { proc, client, frame } = launched;
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("trim-empty-margins-restore: no canvas");
    }
    const layout = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
    const pos = /page n=1 .* pos=-?\d+,-?\d+,(-?\d+),(-?\d+)/.exec(layout);
    if (!pos) {
      throw new Error(`trim-empty-margins-restore: no page rect:\n${layout}`);
    }
    const dx = Number(pos[1]);
    const dy = Number(pos[2]);
    if (dx <= 0 || dy <= 0) {
      throw new Error(`trim-empty-margins-restore: empty page rect ${dx}x${dy}`);
    }
    return { extent: getScrollInfo(canvas).max, aspect: dy / dx };
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("trim-empty-margins-restore");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, buildPdf());

  const off = await pageLayout(dir, pdf, false);
  const on = await pageLayout(dir, pdf, true);
  if (off.extent <= 0 || on.extent <= 0) {
    throw new Error(`trim-empty-margins-restore: no scroll range (off ${off.extent}, on ${on.extent})`);
  }
  // the content is a small wide block on a letter page, so trimming has to
  // change the page shape. Fit page then scales that shape to the window, so
  // the scroll extent is not comparable across window sizes.
  const notShrunk = IS_MAC ? on.aspect > off.aspect / 2 : on.extent > off.extent / 2;
  if (notShrunk) {
    throw new Error(
      `trim-empty-margins-restore: TrimEmptyMargins had no effect ` +
        `(extent ${off.extent} -> ${on.extent}, aspect ${off.aspect.toFixed(3)} -> ${on.aspect.toFixed(3)})`,
    );
  }
  console.log(
    `trim-empty-margins-restore: OK (extent ${off.extent} -> ${on.extent}, aspect ${off.aspect.toFixed(3)} -> ${on.aspect.toFixed(3)})`,
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
