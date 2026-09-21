// Navigate Files in Folder: click the folder path to edit it; Enter navigates
// to a valid directory (or a file's directory), Esc or an invalid path restore
// the previous one (issue #6232, 4th item).
import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { clickAt, killAndWait, launchControlled } from "./win-automation.ts";
import { enumWindows, getWindowText, sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

function findNavWindow(): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowText(hwnd) === "Navigate Files in Folder") {
      found = hwnd;
      return false;
    }
    return true;
  });
  if (!found) {
    throw new Error("navigate files: window not found");
  }
  return found;
}

type NavState = { scan: number; sel: number; items: number; dir: string; name: string };
type EditState = { editing: number; text: string };
let lastNavRaw = "";

async function raw(client: ControlClient, action = "", idx = -1): Promise<string> {
  const res = await client.request(ControlCommand.TestNavFiles, [action, idx]);
  lastNavRaw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`navigate files: ${action}: ${lastNavRaw}`);
  }
  return lastNavRaw;
}

async function navState(client: ControlClient): Promise<NavState> {
  const s = await raw(client);
  const m = /^OK scan=(\d) sel=(-?\d+) items=(\d+) back=\d fwd=\d dir="([^"]*)" name="([^"]*)"$/.exec(s);
  if (!m) {
    throw new Error(`navigate files: could not parse state: ${s}`);
  }
  return { scan: Number(m[1]), sel: Number(m[2]), items: Number(m[3]), dir: m[4]!, name: m[5]! };
}

async function editState(client: ControlClient, action: string): Promise<EditState> {
  const s = await raw(client, action);
  const m = /^OK editing=(\d) text="(.*)"$/.exec(s);
  if (!m) {
    throw new Error(`navigate files: could not parse edit state: ${s}`);
  }
  return { editing: Number(m[1]), text: m[2]! };
}

async function waitNav(client: ControlClient, what: string, pred: (state: NavState) => boolean): Promise<NavState> {
  const deadline = Date.now() + 8000;
  let last: NavState | null = null;
  while (Date.now() < deadline) {
    last = await navState(client);
    if (!last.scan && pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`navigate files: ${what}: state did not settle: ${JSON.stringify(last)} raw=${lastNavRaw}`);
}

function sameDir(a: string, b: string): boolean {
  return a.replace(/[\\/]+$/, "").toLowerCase() === b.replace(/[\\/]+$/, "").toLowerCase();
}

async function expectEdit(client: ControlClient, action: string, editing: number, what: string): Promise<EditState> {
  const s = await editState(client, action);
  if (s.editing !== editing) {
    throw new Error(`navigate files: ${what}: expected editing=${editing}, got ${JSON.stringify(s)}`);
  }
  return s;
}

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6232-path-edit");
  const sub = join(root, "sub");
  rmSync(root, { recursive: true, force: true });
  mkdirSync(sub, { recursive: true });
  copyFileSync(SRC_PDF, join(sub, "a.pdf"));
  copyFileSync(SRC_PDF, join(root, "b.pdf"));

  const { proc, client } = await launchControlled([join(root, "b.pdf")]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNavigateFilesInFolder"]);
    await waitNav(client, "initial", (s) => sameDir(s.dir, root) && s.name === "b.pdf");

    // a real click on the path label starts editing (the label must take the
    // mouse: plain virtual text doesn't); it starts with the current path
    const rectStr = await raw(client, "path-label-rect");
    const rm = /^OK (-?\d+) (-?\d+) (\d+) (\d+)$/.exec(rectStr);
    if (!rm) {
      throw new Error(`navigate files: could not parse label rect: ${rectStr}`);
    }
    const hwnd = findNavWindow();
    await clickAt(hwnd, Number(rm[1]) + Number(rm[3]) / 2, Number(rm[2]) + Number(rm[4]) / 2);
    const e = await expectEdit(client, "path-state", 1, "begin by click");
    if (!sameDir(e.text, root)) {
      throw new Error(`navigate files: edit should start with the current dir, got ${JSON.stringify(e)}`);
    }
    await expectEdit(client, `path-text:${sub}`, 1, "set text");
    await expectEdit(client, "path-commit", 0, "commit");
    await waitNav(client, "commit valid dir", (s) => sameDir(s.dir, sub) && s.items === 2);

    // an invalid path restores the previous dir
    await expectEdit(client, "edit-path", 1, "begin 2");
    await expectEdit(client, `path-text:${join(root, "no-such-dir")}`, 1, "set text 2");
    await expectEdit(client, "path-commit", 0, "commit 2");
    await sleep(300);
    await waitNav(client, "invalid path keeps dir", (s) => sameDir(s.dir, sub));

    // Esc restores too
    await expectEdit(client, "edit-path", 1, "begin 3");
    await expectEdit(client, `path-text:${root}`, 1, "set text 3");
    await expectEdit(client, "path-cancel", 0, "cancel");
    await sleep(300);
    await waitNav(client, "cancel keeps dir", (s) => sameDir(s.dir, sub));

    // a file path (quoted, as Explorer's Copy as path gives it) opens its
    // folder with the file selected
    await expectEdit(client, "edit-path", 1, "begin 4");
    await expectEdit(client, `path-text:"${join(root, "b.pdf")}"`, 1, "set text 4");
    await expectEdit(client, "path-commit", 0, "commit 4");
    await waitNav(client, "commit file path", (s) => sameDir(s.dir, root) && s.name === "b.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(root, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
