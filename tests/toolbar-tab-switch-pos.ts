// Regression test: toolbar Open/Print stay put when switching document <-> Home,
// and a background tab's GoToPage must not crash UpdateScrollbars.
//
// Run: bun tests/toolbar-tab-switch-pos.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { cmdId, copyFromOneDrive, EXE, ROOT, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

function parseToolbarX(layoutOutput: string): number {
  for (const line of layoutOutput.split("\n")) {
    if (line.startsWith("item name=toolbar")) {
      const match = line.match(/rect=(-?\d+),(-?\d+),(\d+),(\d+)/);
      if (match) {
        return parseInt(match[1]!, 10);
      }
    }
  }
  throw new Error(`toolbar item not found in layout:\n${layoutOutput}`);
}

function parseButtonRect(buttonsOutput: string, id: number): { x: number; y: number; dx: number; dy: number } {
  for (const line of buttonsOutput.split("\n")) {
    if (line.includes(`cmd=${id}`)) {
      const match = line.match(/rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/);
      if (match) {
        const x1 = parseInt(match[1]!, 10);
        const y1 = parseInt(match[2]!, 10);
        const x2 = parseInt(match[3]!, 10);
        const y2 = parseInt(match[4]!, 10);
        return { x: x1, y: y1, dx: x2 - x1, dy: y2 - y1 };
      }
    }
  }
  throw new Error(`button cmd=${cmdId} not found in toolbar buttons:\n${buttonsOutput}`);
}

async function waitHiddenTabGoToPage(client: { hiddenTabGoToPage: () => Promise<void> }): Promise<void> {
  const deadline = Date.now() + 5000;
  let lastErr: unknown;
  while (Date.now() < deadline) {
    try {
      await client.hiddenTabGoToPage();
      return;
    } catch (e) {
      lastErr = e;
      await sleep(50);
    }
  }
  throw lastErr instanceof Error ? lastErr : new Error(String(lastErr));
}

export async function testit(): Promise<void> {
  // a big book when there is one, else the small EPUB in the repo
  const mobi = tmpPath("1000.mobi");
  const docPath = copyFromOneDrive("!sumatra/1000.mobi", mobi) ? mobi : join(ROOT, "tests", "issue-5846.epub");
  const cmdOpen = cmdId("CmdOpenFile");
  const cmdPrint = cmdId("CmdPrint");
  await withControlledSumatra(
    EXE,
    async (client) => {
      const [, layoutDoc] = await client.request(ControlCommand.TestLayout, []);
      const [, buttonsDoc] = await client.request(ControlCommand.TestToolbarButtons, []);
      const tbXDoc = parseToolbarX(String(layoutDoc));
      const openDoc = parseButtonRect(String(buttonsDoc), cmdOpen);
      const printDoc = parseButtonRect(String(buttonsDoc), cmdPrint);

      await client.request(ControlCommand.TestInvokeCommand, ["CmdNextTab"]);
      await waitHiddenTabGoToPage(client);

      const [, layoutHome] = await client.request(ControlCommand.TestLayout, []);
      const [, buttonsHome] = await client.request(ControlCommand.TestToolbarButtons, []);
      const tbXHome = parseToolbarX(String(layoutHome));
      const openHome = parseButtonRect(String(buttonsHome), cmdOpen);
      const printHome = parseButtonRect(String(buttonsHome), cmdPrint);

      if (tbXDoc !== tbXHome) {
        throw new Error(`Toolbar X shifted: doc=${tbXDoc} home=${tbXHome}`);
      }

      const openScreenXDoc = tbXDoc + openDoc.x;
      const openScreenXHome = tbXHome + openHome.x;
      if (openScreenXDoc !== openScreenXHome) {
        throw new Error(`Open icon shifted: doc=${openScreenXDoc} home=${openScreenXHome}`);
      }

      const printScreenXDoc = tbXDoc + printDoc.x;
      const printScreenXHome = tbXHome + printHome.x;
      if (printScreenXDoc !== printScreenXHome) {
        throw new Error(`Print icon shifted: doc=${printScreenXDoc} home=${printScreenXHome}`);
      }
    },
    [docPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
