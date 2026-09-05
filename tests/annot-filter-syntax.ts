// Annotation filter conditions (issue #6034): ":a=" author, ":t=" type,
// ":c+" / ":c-" contents, plus plain words. The parsing itself has unit tests
// (AnnotSearch_UnitTests); this checks the filter box really drives the list.
//
// Run: bun tests/annot-filter-syntax.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R 6 0 R] >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /T (kjk) /Contents (alpha note) >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 600 92 620] /T (kjk) /Contents () >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 500 220 530] " +
      "/QuadPoints [72 530 220 530 72 500 220 500] /C [1 1 0] /T (bob) /Contents (beta note) >>",
  ];
  return assemblePdf(objs);
}

function nVisible(raw: string): number {
  const m = /nVisible=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`annot-filter-syntax: no nVisible in\n${raw}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-filter-syntax");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);

    const expectVisible = async (filter: string, want: number) => {
      const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      let got = -1;
      while (Date.now() < deadline) {
        const res = await client.request(ControlCommand.TestAnnotFilter, ["set", filter]);
        const raw = String(res[1] ?? "");
        if (res[0] === 0) {
          got = nVisible(raw);
          if (got === want) {
            return;
          }
        }
        await sleep(50);
      }
      throw new Error(`annot-filter-syntax: '${filter}' showed ${got} annotations, wanted ${want}`);
    };

    await expectVisible("", 3);
    await expectVisible(":c+", 2);
    await expectVisible(":c-", 1);
    await expectVisible(":a=kjk", 2);
    await expectVisible(":a!=kjk", 1);
    await expectVisible(":t=text", 2);
    await expectVisible(":t!=text", 1);
    await expectVisible(":t=text :t=highlight", 3);
    await expectVisible(":a=kjk :c+", 1);
    await expectVisible(":a=kjk beta", 0);
    await expectVisible("note", 2);
    await expectVisible(":t=nosuchtype", 0);
    await expectVisible("", 3);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
