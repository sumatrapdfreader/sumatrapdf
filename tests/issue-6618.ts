// #6618: a restored session's file in a temporary folder that is gone, e.g. a
// PDF attachment Outlook extracted to INetCache\Content.Outlook and deleted
// when it closed, loses its tab silently, whatever KeepMissingSessionTabs
// says, also with LazyLoading. Its file history entry goes too, unless it has
// favorites. Other missing files keep their tab (#5585); with
// KeepMissingSessionTabs = false they're dropped with one warning that counts
// them, not one per file.
//
// Run: bun tests/issue-6618.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, waitForExit } from "./win-automation.ts";
import { enumChildWindows, getClassName, getWindowText, isWindowVisible, sleep } from "./winapi.ts";

const kNotifClass = "SumatraWgDefaultWinClass";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function tabState(path: string): string {
  return `      [
        FilePath = ${path}
        DisplayMode = automatic
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]`;
}

function fileState(path: string, favorite: boolean): string {
  const fav = favorite ? `\n    Favorites [\n      [\n        Name = keep me\n        PageNo = 1\n      ]\n    ]` : "";
  return `  [\n    FilePath = ${path}\n    OpenCount = 5${fav}\n  ]`;
}

interface Session {
  keep: boolean;
  lazy: boolean;
  tabs: string[];
  history: { path: string; favorite: boolean }[];
}

function seedSettings(dir: string, s: Session): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
ReuseInstance = false
UseTabs = true
LazyLoading = ${s.lazy}
KeepMissingSessionTabs = ${s.keep}
FileStates [
${s.history.map((h) => fileState(h.path, h.favorite)).join("\n")}
]
SessionData [
  [
    TabStates [
${s.tabs.map(tabState).join("\n")}
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`,
  );
}

async function tabTexts(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestTabWidths, []))[1] ?? "");
}

// the text of the visible notifications (orig only: ng draws them in the frame)
function notifTexts(frame: number): string[] {
  const res: string[] = [];
  enumChildWindows(frame, (h) => {
    if (getClassName(h) === kNotifClass && isWindowVisible(h)) {
      res.push(getWindowText(h));
    }
    return true;
  });
  return res;
}

async function run(
  name: string,
  s: Session,
  check: (tabs: string, notifs: string[]) => string | null,
  dir: string,
): Promise<string> {
  seedSettings(dir, s);
  const { proc, client, frame } = await launchControlled(["-appdata", dir], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    // the restore can still be adding tabs; the warnings last 3 seconds
    const deadline = Date.now() + 2000;
    let err: string | null = null;
    for (;;) {
      err = check(await tabTexts(client), USE_NG ? [] : notifTexts(frame));
      if (!err || Date.now() > deadline) {
        break;
      }
      await sleep(100);
    }
    if (err) {
      throw new Error(`issue-6618: ${name}: ${err}\ntabs:\n${await tabTexts(client)}`);
    }
    await client.request(ControlCommand.TestInvokeCommand, ["WM_CLOSE"]);
    if (!(await waitForExit(proc))) {
      throw new Error(`issue-6618: ${name}: the window did not close`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  return readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
}

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6618");
  rmSync(root, { recursive: true, force: true });
  mkdirSync(root, { recursive: true });
  const here = join(root, "here.pdf");
  writeFileSync(here, makePdf(), "latin1");
  // missing files: one in a normal folder, an Outlook attachment, one in %TEMP%
  const gone = join(root, "gone.pdf");
  const attach = join(root, "INetCache", "Content.Outlook", "AB12CD34", "attach.pdf");
  const tempGone = join(tmpdir(), `sumatra-6618-${process.pid}.pdf`);
  for (const p of [gone, attach, tempGone]) {
    if (existsSync(p)) {
      throw new Error(`issue-6618: ${p} must not exist`);
    }
  }

  for (const lazy of [false, true]) {
    const dir = join(root, `keep-lazy-${lazy}`);
    mkdirSync(dir, { recursive: true });
    const saved = await run(
      `KeepMissingSessionTabs = true, LazyLoading = ${lazy}`,
      {
        keep: true,
        lazy,
        tabs: [here, gone, attach, tempGone],
        history: [
          { path: gone, favorite: false },
          { path: attach, favorite: false },
          { path: tempGone, favorite: true },
        ],
      },
      (tabs) => {
        if (!tabs.includes("here") || !tabs.includes("gone")) {
          return "the present file's or the missing normal file's tab is missing";
        }
        if (tabs.includes("attach") || tabs.includes("sumatra-6618")) {
          return "a missing temporary file kept its tab";
        }
        return null;
      },
      dir,
    );
    if (saved.includes(attach)) {
      throw new Error(`issue-6618: lazy=${lazy}: the gone attachment is still in the file history`);
    }
    if (!saved.includes(tempGone)) {
      throw new Error(`issue-6618: lazy=${lazy}: a gone temporary file with favorites lost its history entry`);
    }
    if (!saved.includes(gone)) {
      throw new Error(`issue-6618: lazy=${lazy}: the missing normal file left the file history`);
    }
  }

  // KeepMissingSessionTabs = false: one warning for the two normal files, none
  // for the attachment
  const gone2 = join(root, "gone2.pdf");
  const dir = join(root, "drop");
  mkdirSync(dir, { recursive: true });
  await run(
    "KeepMissingSessionTabs = false",
    { keep: false, lazy: false, tabs: [here, gone, attach, gone2], history: [] },
    (tabs, notifs) => {
      if (!tabs.includes("here") || tabs.includes("gone") || tabs.includes("attach")) {
        return "missing files kept their tab";
      }
      if (USE_NG) {
        return null;
      }
      const missing = notifs.filter((t) => t.includes("not found"));
      if (missing.length !== 1 || !missing[0]!.includes("2 files of the session were not found")) {
        return `want one warning counting 2 files, got ${JSON.stringify(notifs)}`;
      }
      return null;
    },
    dir,
  );
  console.log("issue-6618: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
