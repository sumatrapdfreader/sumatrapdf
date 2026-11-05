// CmdGoToNextChapter / CmdGoToPrevChapter move between bookmark (ToC) entries.
// Next goes to the first entry after the current page, Previous to the last one
// before it, so from inside a chapter Previous goes to that chapter's start.
//
// Run: bun tests/chapter-nav.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { postMessage, sleep, WM_KEYDOWN } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kPages = 10;
const kWaitMs = 5000;
const VK_MEDIA_NEXT_TRACK = 0xb0;

// a media key bound to a chapter command, the way the post asked for
const kSettings = `RestoreSession = false
CheckForUpdates = false
Shortcuts [
    [
        Cmd = CmdGoToNextChapter
        Key = Media Next
    ]
]
`;

// bookmarks: Ch1 -> 1 (with Sec 1.1 -> 2), Ch2 -> 5, Ch3 -> 8
function makeChaptersPdf(): string {
  const firstPage = 8;
  const page = (n: number) => `${firstPage + n - 1} 0 R`;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R /Outlines 3 0 R >>";
  const kids = Array.from({ length: kPages }, (_, i) => page(i + 1)).join(" ");
  objs[2] = `<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`;
  objs[3] = "<< /Type /Outlines /First 4 0 R /Last 7 0 R /Count 3 >>";
  objs[4] = `<< /Title (Ch1) /Parent 3 0 R /Next 6 0 R /First 5 0 R /Last 5 0 R /Count 1 /Dest [${page(1)} /Fit] >>`;
  objs[5] = `<< /Title (Sec 1.1) /Parent 4 0 R /Dest [${page(2)} /Fit] >>`;
  objs[6] = `<< /Title (Ch2) /Parent 3 0 R /Prev 4 0 R /Next 7 0 R /Dest [${page(5)} /Fit] >>`;
  objs[7] = `<< /Title (Ch3) /Parent 3 0 R /Prev 6 0 R /Dest [${page(8)} /Fit] >>`;
  for (let i = 1; i <= kPages; i++) {
    objs[firstPage + i - 1] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>";
  }
  return assemblePdf(objs.slice(1));
}

async function waitForPage(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  let page = 0;
  while (Date.now() < deadline) {
    page = (await client.chapterInfo()).page;
    if (page === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`chapter-nav: ${what}: on page ${page}, want ${want}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("chapter-nav");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "chapters.pdf");
  writeFileSync(pdf, makeChaptersPdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), kSettings);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const next = cmdId("CmdGoToNextChapter");
    const prev = cmdId("CmdGoToPrevChapter");

    // [start page or 0 to stay, command, expected page]
    const steps: [number, number, number][] = [
      [3, next, 5],
      [0, next, 8],
      [0, next, 8], // no entry after the last one
      [0, prev, 5],
      [6, prev, 5], // inside Ch2: back to its start
      [0, prev, 2], // nested entries count too
      [0, prev, 1],
      [0, prev, 1],
    ];
    for (const [start, cmd, want] of steps) {
      if (start) {
        await client.goToLocation(1, start);
        await waitForPage(client, start, `go to page ${start}`);
      }
      const from = (await client.chapterInfo()).page;
      sendCommand(frame, cmd);
      await waitForPage(client, want, `${cmd === next ? "next" : "previous"} chapter from page ${from}`);
    }

    // the Media Next key runs the command it is bound to
    postMessage(findCanvas(frame), WM_KEYDOWN, VK_MEDIA_NEXT_TRACK, 0);
    await waitForPage(client, 2, "Media Next from page 1");
    console.log("chapter-nav: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
