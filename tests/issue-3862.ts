// issue #3862: in single page view a half-page scroll at a page edge turns the
// page, the way a full page scroll does. The half-page commands only turned
// the page in continuous mode, so at fit page they did nothing at all.
//
// Run: bun tests/issue-3862.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PAGE_COUNT = 5;
const START_PAGE = 3;

function makePdf(): string {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>"];
  const kids: string[] = [];
  for (let i = 0; i < PAGE_COUNT; i++) {
    kids.push(`${3 + i} 0 R`);
  }
  objs.push(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${PAGE_COUNT} >>`);
  for (let i = 0; i < PAGE_COUNT; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const m = /page=(\d+)/.exec(String(res[1] ?? ""));
  return m ? Number(m[1]) : -1;
}

async function halfPage(frame: number, client: ControlClient, cmd: string): Promise<number> {
  sendCommandSync(frame, cmdId(cmd));
  await sleep(300);
  return currentPage(client);
}

function expectPage(what: string, got: number, want: number): void {
  if (got !== want) {
    throw new Error(`#3862: ${what}: page ${got}, expected ${want}`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3862.pdf");
  writeFileSync(pdf, makePdf());

  // fit page: nothing to scroll, so every half-page step is a page turn
  {
    const args = ["-view", "single page", "-zoom", "fit page", "-page", String(START_PAGE), pdf];
    const { proc, client, frame } = await launchControlled(args);
    try {
      await sleep(1000);
      expectPage("start", await currentPage(client), START_PAGE);
      expectPage("fit page, half-page up", await halfPage(frame, client, "CmdScrollUpHalfPage"), START_PAGE - 1);
      expectPage("fit page, half-page down", await halfPage(frame, client, "CmdScrollDownHalfPage"), START_PAGE);
    } finally {
      await client.quit().catch(() => {});
      await killAndWait(proc);
    }
  }

  // zoomed in: the first step up is at the top edge and turns the page; a
  // step down from there scrolls within the new page
  {
    const args = ["-view", "single page", "-zoom", "200", "-page", String(START_PAGE), pdf];
    const { proc, client, frame } = await launchControlled(args);
    try {
      await sleep(1000);
      expectPage("200%, half-page up at top", await halfPage(frame, client, "CmdScrollUpHalfPage"), START_PAGE - 1);
      expectPage(
        "200%, half-page down scrolls",
        await halfPage(frame, client, "CmdScrollDownHalfPage"),
        START_PAGE - 1,
      );
    } finally {
      await client.quit().catch(() => {});
      await killAndWait(proc);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
