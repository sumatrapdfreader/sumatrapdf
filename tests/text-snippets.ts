// TextSnippets setting: each snippet is a command (context menu, command
// palette, optional Key) that inserts its text as a free text annotation.
// \n in Text starts a new line, and the box fits every line.
//
// Run: bun tests/text-snippets.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { getClassName, getFocusedHwnd, getRootWindow, postMessage, sendText, sleep, WM_KEYDOWN } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const VK_RETURN = 0x0d;
const VK_ESCAPE = 0x1b;

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
TextSnippets [
	[
		Name = Snippet One
		Text = Approved
	]
	[
		Name = Snippet Two
		Text = Approved\\nJ. Doe
	]
]
`;

async function paletteItems(client: ControlClient, queryLen: number): Promise<number> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const m = /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+)/.exec(String(res[1] ?? ""));
    if (m && +m[2]! === queryLen) {
      return +m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`text-snippets: palette state: ${res[1]}`);
    }
    await sleep(50);
  }
}

// types query into a new palette; Enter runs the only match, else Esc closes it
async function runFromPalette(client: ControlClient, frame: number, query: string): Promise<number> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  let edit = 0;
  for (let i = 0; i < 100 && !edit; i++) {
    const h = getFocusedHwnd(frame);
    if (h && getClassName(h) === "Edit" && getRootWindow(h) !== frame) {
      edit = h;
    }
    await sleep(50);
  }
  if (!edit) {
    throw new Error("text-snippets: palette did not open");
  }
  sendText(edit, query);
  const n = await paletteItems(client, query.length);
  postMessage(edit, WM_KEYDOWN, n === 1 ? VK_RETURN : VK_ESCAPE, 0);
  await sleep(300);
  return n;
}

async function freeTextHeights(client: ControlClient): Promise<number[]> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  return [...raw.matchAll(/type=FreeText page=\d+ rect=[^,]+,[^,]+,[^,]+,([^ ]+)/g)].map((m) => +m[1]!);
}

async function waitFreeTexts(client: ControlClient, n: number): Promise<number[]> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const h = await freeTextHeights(client);
    if (h.length === n) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`text-snippets: ${h.length} free text annotations, want ${n}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("text-snippets");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "blank.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    ]),
    "latin1",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // only the snippets are commands, not the bare Insert Text Snippet
    if ((await runFromPalette(client, frame, ">Insert Text Snippet")) !== 0) {
      throw new Error("text-snippets: the bare Insert Text Snippet command is listed");
    }

    if ((await runFromPalette(client, frame, ">Snippet One")) !== 1) {
      throw new Error("text-snippets: Snippet One is not a palette command");
    }
    const [oneLine] = await waitFreeTexts(client, 1);
    if ((await runFromPalette(client, frame, ">Snippet Two")) !== 1) {
      throw new Error("text-snippets: Snippet Two is not a palette command");
    }
    const heights = await waitFreeTexts(client, 2);
    const twoLines = heights.find((h) => h !== oneLine) ?? 0;
    if (!(twoLines > oneLine * 1.5)) {
      throw new Error(`text-snippets: a two-line snippet is ${twoLines} tall, a one-line one ${oneLine}`);
    }
    console.log("text-snippets: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
