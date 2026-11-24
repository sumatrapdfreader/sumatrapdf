// The page context menu without a selection has orig's rows in ng too: Selection
// lists Select All once and Trim Pages To Selection, hides the selection-only
// commands; the menu has Show > Thumbnails and no Annotations > Create From Selection.
// The Show submenu checks the parts of the window that are shown.
//
// Run: bun tests/context-menu-parity.ts [-ng] [--no-build]

import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { findCanvas, killAndWait, launchControlled, readContextMenuTree } from "./win-automation.ts";
import type { MenuItem } from "./winapi.ts";

const PDF = join(ROOT, "docs", "test", "zlib.3.pdf");

// "depth title" rows, depth-first
type Row = { depth: number; title: string; checked: boolean };

function rowsFromTree(items: MenuItem[], depth = 0, out: Row[] = []): Row[] {
  for (const it of items) {
    out.push({ depth, title: it.text.split("\t")[0]!.replace(/&/g, ""), checked: it.checked === true });
    if (it.items) {
      rowsFromTree(it.items, depth + 1, out);
    }
  }
  return out;
}

// TestContextMenuAt: path, kind, cmd, disabled, checked, title, accel
function rowsFromNg(raw: string): Row[] {
  const out: Row[] = [];
  for (const line of raw.split("\n")) {
    const c = line.split("\t");
    if (c.length < 6 || c[1] === "-") {
      continue;
    }
    out.push({ depth: c[0]!.split(".").length - 1, title: c[5]!, checked: c[4] === "1" });
  }
  return out;
}

async function contextMenuRows(client: ControlClient, frame: number): Promise<Row[]> {
  if (!USE_NG) {
    return rowsFromTree(await readContextMenuTree(findCanvas(frame)));
  }
  const lay = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const m = /item name=canvas visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(lay);
  if (!m) {
    throw new Error(`context-menu-parity: no canvas in the layout:\n${lay}`);
  }
  const x = Number(m[1]) + Math.floor(Number(m[3]) / 2);
  const y = Number(m[2]) + Math.floor(Number(m[4]) / 2);
  return rowsFromNg(String((await client.request(ControlCommand.TestContextMenuAt, [x, y]))[1] ?? ""));
}

// the rows of the top-level submenu `title`
function submenu(rows: Row[], title: string): string[] {
  const i = rows.findIndex((r) => r.depth === 0 && r.title === title);
  if (i < 0) {
    return [];
  }
  const out: string[] = [];
  for (let j = i + 1; j < rows.length && rows[j]!.depth > 0; j++) {
    if (rows[j]!.depth === 1) {
      out.push(rows[j]!.title);
    }
  }
  return out;
}

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.request(ControlCommand.WaitRenderIdle, []);
    const rows = await contextMenuRows(client, frame);
    const dump = rows.map((r) => `${"  ".repeat(r.depth)}${r.title}`).join("\n");
    const fail = (why: string) => {
      throw new Error(`context-menu-parity: ${why}\n${dump}`);
    };

    const sel = submenu(rows, "Selection");
    if (sel.filter((t) => t === "Select All").length !== 1) {
      fail("Selection should list Select All once");
    }
    if (!sel.includes("Trim Pages to Selection")) {
      fail("Selection lacks Trim Pages to Selection");
    }
    for (const t of ["Copy to Clipboard", "Visual Search with Google Lens", "Zoom to Selection"]) {
      if (sel.includes(t)) {
        fail(`Selection shows '${t}' without a selection`);
      }
    }
    if (!submenu(rows, "Show").includes("Thumbnails")) {
      fail("no Show > Thumbnails");
    }
    if (submenu(rows, "Annotations").includes("Create from Selection")) {
      fail("Annotations shows Create from Selection without a selection");
    }

    // the settings toggles show their state: the caption is shown by default
    // an item of the top-level Show submenu
    const isChecked = (rs: Row[], title: string) => {
      const i = rs.findIndex((r) => r.depth === 0 && r.title === "Show");
      for (let j = i + 1; i >= 0 && j < rs.length && rs[j]!.depth > 0; j++) {
        if (rs[j]!.depth === 1 && rs[j]!.title === title) {
          return rs[j]!.checked;
        }
      }
      return undefined;
    };
    // no client title bar off Windows, so CmdToggleCaption is not in the menu
    if (IS_WIN && isChecked(rows, "Caption") !== true) {
      fail("Show > Caption should be checked");
    }
    if (isChecked(rows, "Auto-Hide Sidebar") !== false) {
      fail("Show > Auto-Hide Sidebar should start unchecked");
    }
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleSidebarAutoHide"]);
    try {
      const rows2 = await contextMenuRows(client, frame);
      if (isChecked(rows2, "Auto-Hide Sidebar") !== true) {
        fail("Show > Auto-Hide Sidebar should be checked once it's on");
      }
    } finally {
      await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleSidebarAutoHide"]);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
