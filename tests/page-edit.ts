// Moving pages and inserting pages from another PDF, with undo / redo and
// save. Each page of the test PDF has a unique width (601, 602, ...) so the
// page order can be read back; a bookmark and an annotation must follow
// their page.
//
// Run: bun tests/page-edit.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

// n pages of widths first, first+1, ...; bookmark "Target" to page tocPage;
// a square annotation on page annotPage (0: none)
export function makePdf(n: number, first: number, tocPage: number, annotPage: number): string {
  const objs: string[] = [];
  const pageObj = (i: number) => 5 + i; // objects 5.. are the pages
  objs[1] = `<< /Type /Catalog /Pages 2 0 R${tocPage ? " /Outlines 3 0 R" : ""} >>`;
  const kids = Array.from({ length: n }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  objs[2] = `<< /Type /Pages /Count ${n} /Kids [${kids}] >>`;
  objs[3] = "<< /Type /Outlines /First 4 0 R /Last 4 0 R /Count 1 >>";
  objs[4] = tocPage ? `<< /Title (Target) /Parent 3 0 R /Dest [${pageObj(tocPage - 1)} 0 R /Fit] >>` : "<< >>";
  const annotObj = 5 + n;
  for (let i = 0; i < n; i++) {
    const annots = i + 1 === annotPage ? ` /Annots [${annotObj} 0 R]` : "";
    objs[pageObj(i)] = `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${first + i} 792]${annots} >>`;
  }
  objs[annotObj] = `<< /Type /Annot /Subtype /Square /Rect [72 420 192 540] /C [1 0 0] >>`;
  return assemblePdf(objs.slice(1));
}

type State = { pages: number; widths: number[]; toc: string; raw: string };

async function pageEdit(client: ControlClient, action = "", arg = "", before = 0): Promise<State> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageEdit, [action, arg, before]);
    const raw = String(res[1] ?? "");
    const m = /pages=(\d+) widths=([\d,]+) toc=(\S*)/.exec(raw);
    if (res[0] === 0 && m) {
      return { pages: +m[1]!, widths: m[2]!.split(",").map(Number), toc: m[3]!, raw };
    }
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`page-edit: TestPageEdit: ${raw}`);
    }
    await sleep(100);
  }
}

function want(s: State, widths: number[], tocPage: number, what: string) {
  if (s.widths.join(",") !== widths.join(",") || s.toc !== `Target:${tocPage};`) {
    throw new Error(`page-edit: ${what}: got ${s.raw}, want widths=${widths.join(",")} toc=Target:${tocPage};`);
  }
}

async function squarePage(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  return +(/type=Square page=(\d+)/.exec(raw)?.[1] ?? 0);
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`page-edit: ${what}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("page-edit");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 3, 2), "latin1");
  const other = join(dir, "other.pdf");
  writeFileSync(other, makePdf(2, 701, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    let s = await pageEdit(client);
    want(s, [601, 602, 603, 604], 3, "as opened");

    // page 1 to the slot in front of page 4
    s = await pageEdit(client, "move", "1", 4);
    want(s, [602, 603, 601, 604], 2, "after moving page 1");
    await waitFor("the annotation didn't follow its page", async () => (await squarePage(client)) === 1);

    sendCommand(frame, cmdId("CmdUndo"));
    await waitFor("undo didn't restore the order", async () => (await pageEdit(client)).widths[0] === 601);
    want(await pageEdit(client), [601, 602, 603, 604], 3, "after undo");
    sendCommand(frame, cmdId("CmdRedo"));
    await waitFor("redo didn't move it again", async () => (await pageEdit(client)).widths[0] === 602);

    // pages 1 and 2 are in front already: nothing to move
    s = await pageEdit(client, "move", "1,2", 1);
    if (s.raw.includes("moved=1")) {
      throw new Error(`page-edit: moving pages already in front must be a no-op: ${s.raw}`);
    }
    // pages 2 and 4 (603, 604) to the front, keeping their order
    s = await pageEdit(client, "move", "2,4", 1);
    want(s, [603, 604, 602, 601], 1, "after moving pages 2 and 4 to the front");

    // two pages of another PDF in front of page 2
    s = await pageEdit(client, "insert", other, 2);
    if (!s.raw.includes("inserted=2")) {
      throw new Error(`page-edit: insert: ${s.raw}`);
    }
    want(s, [603, 701, 702, 604, 602, 601], 1, "after inserting");
    sendCommand(frame, cmdId("CmdUndo"));
    await waitFor("undo didn't remove the inserted pages", async () => (await pageEdit(client)).pages === 4);

    // saved and reloaded: the order is in the file
    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    await waitFor(
      "the saved file doesn't have the new order",
      async () => (await pageEdit(client)).widths.join(",") === "603,604,602,601",
    );
    await sleep(500);
    want(await pageEdit(client), [603, 604, 602, 601], 1, "after save and reload");
    console.log("page-edit: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
