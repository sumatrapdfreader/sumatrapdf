// #6234: with the home page's thumbnail band scrolled to the very bottom (End),
// the captions of the last row must not be cut off by the band's bottom edge.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { setCursorPos, sleep } from "./winapi";
import { launchControlled, killAndWait, sendCommand } from "./win-automation";
import type { ControlClient, HomeSelection } from "./control.ts";

const nFiles = 12;

function makeAppDir(): string {
  const dir = tmpPath("issue-6234");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nShowTips = true\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

function sameRect(a: number[], b: number[]): boolean {
  return a.length === b.length && a.every((v, i) => v === b[i]);
}

async function waitForHome(
  client: ControlClient,
  pred: (h: HomeSelection) => boolean,
  what: string,
  timeoutMs = 8000,
): Promise<HomeSelection> {
  const deadline = Date.now() + timeoutMs;
  let last: HomeSelection | null = null;
  for (;;) {
    last = await client.homeSelection();
    if (last.ready && pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6234: ${what} (last: ${last.raw})`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  setCursorPos(0, 0);
  // compact window so the files overflow the thumbs band and End has to scroll
  const { proc, client, frame } = await launchControlled(["-appdata", makeAppDir(), "-window-pos", "820x500@20x20"]);
  try {
    const before = await waitForHome(
      client,
      (h) => h.entries === nFiles && h.thumbsArea[3]! > 0 && h.lastCaption[3]! > 0,
      "home page never listed the files",
    );
    const areaBottom = before.thumbsArea[1]! + before.thumbsArea[3]!;
    const captionBottomBefore = before.lastCaption[1]! + before.lastCaption[3]!;
    if (captionBottomBefore <= areaBottom) {
      throw new Error(`issue-6234: window too tall, the last row already fits: ${before.raw}`);
    }

    sendCommand(frame, cmdId("CmdGoToLastPage"));
    // the scroll is applied on the next layout; wait for the band to move and settle
    let h = await waitForHome(
      client,
      (s) => s.entries === nFiles && !sameRect(s.lastCaption, before.lastCaption),
      "End did not scroll the thumbnail band",
    );
    for (;;) {
      await sleep(60);
      const cur = await waitForHome(client, (s) => s.entries === nFiles, "home page lost its files");
      if (sameRect(cur.lastCaption, h.lastCaption)) {
        h = cur;
        break;
      }
      h = cur;
    }

    const captionBottom = h.lastCaption[1]! + h.lastCaption[3]!;
    const bottom = h.thumbsArea[1]! + h.thumbsArea[3]!;
    if (captionBottom > bottom) {
      throw new Error(
        `issue-6234: last row caption cut off: caption bottom ${captionBottom} > thumbs area bottom ${bottom}: ${h.raw}`,
      );
    }
    console.log("issue-6234: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
