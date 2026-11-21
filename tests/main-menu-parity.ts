// Main menu state that ng must share with orig: no Read Aloud menu on the home
// page; View > Show Whole Pages only with a user crop and Read Columns: Show
// Whole Page only in read-columns mode, so both are disabled on a fresh PDF.
//
// Run: bun tests/main-menu-parity.ts [-ng] [--no-build]

import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, readMenuBarTree } from "./win-automation.ts";
import type { MenuItem } from "./winapi.ts";

const PDF = join(ROOT, "docs", "test", "zlib.3.pdf");

type Row = { title: string; disabled: boolean };

function rowsFromTree(items: MenuItem[], out: Row[] = []): Row[] {
  for (const it of items) {
    out.push({ title: it.text.split("\t")[0]!.replace(/&/g, ""), disabled: !!it.disabled });
    if (it.items) {
      rowsFromTree(it.items, out);
    }
  }
  return out;
}

// every row of the menu bar, depth-first
async function menuRows(client: ControlClient, frame: number): Promise<Row[]> {
  if (!USE_NG) {
    return rowsFromTree((await readMenuBarTree(frame)).flat());
  }
  // TestMainMenu: path, kind, cmd, disabled, checked, title, accel
  const raw = String((await client.request(ControlCommand.TestMainMenu, []))[1] ?? "");
  const out: Row[] = [];
  for (const line of raw.split("\n")) {
    const c = line.split("\t");
    if (c.length >= 6 && c[1] !== "-") {
      out.push({ title: c[5]!, disabled: c[3] === "1" });
    }
  }
  return out;
}

function dump(rows: Row[]): string {
  return rows.map((r) => `${r.title}${r.disabled ? " (disabled)" : ""}`).join("\n");
}

async function checkHome(): Promise<void> {
  const { proc, client, frame } = await launchControlled([]);
  try {
    const rows = await menuRows(client, frame);
    if (rows.some((r) => r.title === "Start Reading From Top")) {
      throw new Error(`main-menu-parity: home page has a Read Aloud menu\n${dump(rows)}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function checkPdf(): Promise<void> {
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.request(ControlCommand.WaitRenderIdle, []);
    const rows = await menuRows(client, frame);
    for (const title of ["Show Whole Pages", "Read Columns: Show Whole Page"]) {
      const r = rows.find((x) => x.title === title);
      if (!r || !r.disabled) {
        throw new Error(`main-menu-parity: '${title}' should be disabled\n${dump(rows)}`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await checkHome();
  await checkPdf();
}

if (import.meta.main) {
  await runStandalone(testit);
}
