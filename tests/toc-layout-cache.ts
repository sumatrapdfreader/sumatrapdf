// ng laid out every Bookmarks row (wrap estimate, page label) on every frame,
// slow for a big ToC. The rows are now laid out only when they or what their
// heights depend on change: frames from scrolling must not lay them out again,
// collapsing a bookmark must.
//
// ng only: it counts the layouts through -dbg-control.
// Run: bun tests/toc-layout-cache.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kTopItems = 60;
const kChildren = 3;
const kBurst = 20;

// 2 pages; kTopItems bookmarks, the first one open with kChildren children
function makePdf(): string {
  const top = (i: number) => 6 + i;
  const child = (i: number) => 6 + kTopItems + i;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R /PageMode /UseOutlines >>`,
    `<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Outlines /Count ${kTopItems + kChildren} /First ${top(0)} 0 R /Last ${top(kTopItems - 1)} 0 R >>`,
  ];
  for (let i = 0; i < kTopItems; i++) {
    let o = `<< /Title (Bookmark ${i} with a somewhat long title) /Parent 5 0 R /Dest [3 0 R /XYZ null null null]`;
    if (i > 0) o += ` /Prev ${top(i - 1)} 0 R`;
    if (i < kTopItems - 1) o += ` /Next ${top(i + 1)} 0 R`;
    if (i === 0) o += ` /First ${child(0)} 0 R /Last ${child(kChildren - 1)} 0 R /Count ${kChildren}`;
    objs.push(o + " >>");
  }
  for (let i = 0; i < kChildren; i++) {
    let o = `<< /Title (Child ${i}) /Parent ${top(0)} 0 R /Dest [4 0 R /XYZ null null null]`;
    if (i > 0) o += ` /Prev ${child(i - 1)} 0 R`;
    if (i < kChildren - 1) o += ` /Next ${child(i + 1)} 0 R`;
    objs.push(o + " >>");
  }
  return assemblePdf(objs);
}

async function tocLayouts(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, ["toclayouts", 0]))[1] ?? "");
  const m = /\blayouts=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`toc-layout-cache: bad toclayouts reply: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP toc-layout-cache: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("toc-layout-cache");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "toc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "ShowToc = true\nShowFavorites = false\nRestoreSession = false\nCheckForUpdates = false\n",
  );

  const { proc, client } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await pollUntil(
      async () => String((await client.request(ControlCommand.TestUiState, ["count"]))[1] ?? ""),
      (s) => /tocRows=(\d+)/.exec(s)?.[1] === String(kTopItems + kChildren),
      { error: (s) => `toc-layout-cache: bookmarks not shown: ${s}` },
    );
    // let the sidebar's width settle
    await sleep(500);
    const before = await tocLayouts(client);
    for (let i = 0; i < kBurst; i++) {
      await client.request(ControlCommand.TestInvokeCommand, [i % 2 === 0 ? "CmdScrollDown" : "CmdScrollUp"]);
    }
    await client.waitForRenderIdle();
    const inBurst = (await tocLayouts(client)) - before;
    console.log(`toc-layout-cache: ${inBurst} ToC layouts for ${kBurst} scroll commands`);
    if (inBurst > 0) {
      throw new Error(`toc-layout-cache: ${inBurst} ToC layouts for ${kBurst} scroll commands, want 0`);
    }

    // fewer rows must be laid out again
    const beforeCollapse = await tocLayouts(client);
    await client.request(ControlCommand.TestUiState, ["collapse-roots", 0]);
    await pollUntil(
      async () => tocLayouts(client),
      (n) => n > beforeCollapse,
      { error: () => "toc-layout-cache: collapsing a bookmark didn't lay the rows out again" },
    );
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("toc-layout-cache: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
