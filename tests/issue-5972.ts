// #5972: Command Palette should support the Find window's navigation keys.
// Ctrl+A was eaten as CmdSelectAll (document select) and did not select the
// query. Ctrl+C was eaten as CmdCopySelection and did not copy the query.
// Home/End/PageUp/PageDown never moved the list.
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone } from "./util";
import {
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  postMessage,
  sendText,
  sleep,
  WM_CLOSE,
  WM_KEYDOWN,
} from "./winapi";
import { killAndWait, launchControlled, sendCommand, sendCommandSync, waitForExit } from "./win-automation";

const VK_END = 0x23;
const VK_HOME = 0x24;
const VK_NEXT = 0x22; // Page Down

function clipboardText(): string {
  const res = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", "Get-Clipboard -Raw"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (res.exitCode !== 0) {
    throw new Error(`issue-5972: Get-Clipboard failed: ${res.stderr.toString()}`);
  }
  return res.stdout.toString().replace(/\r?\n$/, "");
}

function setClipboard(value: string): void {
  const res = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", `Set-Clipboard -Value '${value}'`], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (res.exitCode !== 0) {
    throw new Error(`issue-5972: Set-Clipboard failed: ${res.stderr.toString()}`);
  }
}

type PaletteState = { sel: number; items: number; querySel: [number, number]; queryLen: number };

async function paletteState(client: ControlClient): Promise<PaletteState | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-5972: TestCommandPalette failed: ${out}`);
  }
  const m = /OK sel=(-?\d+) items=(\d+) querySel=(-?\d+),(-?\d+) queryLen=(\d+)/.exec(out);
  if (!m) {
    throw new Error(`issue-5972: could not parse: ${out}`);
  }
  return { sel: +m[1]!, items: +m[2]!, querySel: [+m[3]!, +m[4]!], queryLen: +m[5]! };
}

async function waitPalette(client: ControlClient): Promise<PaletteState> {
  const deadline = Date.now() + 8_000;
  for (;;) {
    const st = await paletteState(client);
    if (st && st.items > 1) {
      return st;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-5972: command palette did not open");
    }
    await sleep(50);
  }
}

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));
    let st = await waitPalette(client);
    const { palette, edit } = findPalette(frame);
    if (!palette || !edit) {
      throw new Error("issue-5972: no command palette edit");
    }

    postMessage(edit, WM_KEYDOWN, VK_NEXT, 0);
    const afterPage = await waitForSelChange(client, st.sel, "PageDown");
    if (afterPage.sel <= st.sel) {
      throw new Error(`issue-5972: PageDown did not move down: ${JSON.stringify(afterPage)}`);
    }

    // posted to the palette window (not the edit), so Home/End always move the list
    postMessage(palette, WM_KEYDOWN, VK_END, 0);
    await waitForSel(client, afterPage.items - 1, "End");
    postMessage(palette, WM_KEYDOWN, VK_HOME, 0);
    await waitForSel(client, 0, "Home");

    const query = "palette-copy-5972";
    sendText(edit, query);
    sendCommandSync(palette, cmdId("CmdSelectAll"));
    const deadline = Date.now() + 3_000;
    for (;;) {
      st = (await paletteState(client))!;
      if (st.queryLen === query.length && st.querySel[0] === 0 && st.querySel[1] === query.length) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-5972: Ctrl+A did not select the query: ${JSON.stringify(st)}`);
      }
      await sleep(40);
    }

    const sentinel = "issue-5972 clipboard sentinel";
    setClipboard(sentinel);
    sendCommandSync(palette, cmdId("CmdCopySelection"));
    const copyDeadline = Date.now() + 3_000;
    for (;;) {
      const copied = clipboardText();
      if (copied === query) {
        break;
      }
      if (Date.now() > copyDeadline) {
        throw new Error(`issue-5972: Ctrl+C did not copy the query (clipboard=${JSON.stringify(copied)})`);
      }
      await sleep(40);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5972: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5972: OK");
}

async function waitForSelChange(client: ControlClient, from: number, what: string): Promise<PaletteState> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const st = await paletteState(client);
    if (st && st.sel !== from) {
      return st;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5972: ${what} did not change selection (sel=${st?.sel})`);
    }
    await sleep(40);
  }
}

async function waitForSel(client: ControlClient, want: number, what: string): Promise<PaletteState> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const st = await paletteState(client);
    if (st && st.sel === want) {
      return st;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5972: ${what} (sel=${st?.sel}, want=${want})`);
    }
    await sleep(40);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
