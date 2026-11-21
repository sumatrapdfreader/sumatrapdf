// The page context menu without a selection has orig's rows in ng too: Selection
// lists Select All once and Trim Pages To Selection, hides the selection-only
// commands; the menu has Show Thumbnails and no Annotations > Create From Selection.
//
// Run: bun tests/context-menu-parity.ts [-ng] [--no-build]

import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { findCanvas, killAndWait, launchControlled, readContextMenuTree } from "./win-automation.ts";
import type { MenuItem } from "./winapi.ts";

const PDF = join(ROOT, "docs", "test", "zlib.3.pdf");

// "depth title" rows, depth-first
type Row = { depth: number; title: string };

function rowsFromTree(items: MenuItem[], depth = 0, out: Row[] = []): Row[] {
  for (const it of items) {
    out.push({ depth, title: it.text.split("\t")[0]!.replace(/&/g, "") });
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
    out.push({ depth: c[0]!.split(".").length - 1, title: c[5]! });
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
    if (!sel.includes("Trim Pages To Selection")) {
      fail("Selection lacks Trim Pages To Selection");
    }
    for (const t of ["Copy To Clipboard", "Visual Search With Google Lens", "Zoom To Selection"]) {
      if (sel.includes(t)) {
        fail(`Selection shows '${t}' without a selection`);
      }
    }
    if (!rows.some((r) => r.depth === 0 && r.title === "Show Thumbnails")) {
      fail("no Show Thumbnails");
    }
    if (submenu(rows, "Annotations").includes("Create From Selection")) {
      fail("Annotations shows Create From Selection without a selection");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
