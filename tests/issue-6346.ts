// Discussion https://github.com/sumatrapdfreader/sumatrapdf/discussions/6346
//
// Page grid slant is measured from the right (90 upright). Dragging either
// spacing keeps cells square; cancelling restores both dimensions.
//
// Run: bun tests/issue-6346.ts [--no-build]

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { makePageGridPdf } from "./issue-4398.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const PDF = join(ROOT, "tests", "issue-4398.pdf");

function num(s: string, name: string): number {
  const m = new RegExp(`${name}=(-?\\d+(?:\\.\\d+)?)`).exec(s);
  if (!m) {
    throw new Error(`issue-6346: no ${name} in ${s}`);
  }
  return Number(m[1]);
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    writeFileSync(PDF, makePageGridPdf());
  }
  const appdata = tmpPath("issue-6346-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle(30000);
      await client.request(ControlCommand.TestCanvasFlags, ["set-grid", 1]);
      const reset = String((await client.request(ControlCommand.TestCanvasFlags, ["reset-grid"]))[1] ?? "");
      if (!/slant=90/.test(reset)) {
        throw new Error(`issue-6346: default slant is not 90: ${reset}`);
      }

      const upright = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      const dxUpright = Math.abs(num(upright, "x0") - num(upright, "x1"));
      if (dxUpright > 2) {
        throw new Error(`issue-6346: upright grid leans (${dxUpright}px): ${upright}`);
      }

      const slantedState = String((await client.request(ControlCommand.TestCanvasFlags, ["set-slant", 55]))[1] ?? "");
      if (!/slant=55/.test(slantedState)) {
        throw new Error(`issue-6346: set-slant 55: ${slantedState}`);
      }
      const slanted = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      const dxSlant = Math.abs(num(slanted, "x0") - num(slanted, "x1"));
      if (dxSlant < 8) {
        throw new Error(`issue-6346: 55° grid did not lean (${dxSlant}px): ${slanted}`);
      }
      if (num(slanted, "x0") <= num(slanted, "x1")) {
        throw new Error(`issue-6346: 55° grid did not lean right: ${slanted}`);
      }
      await client.request(ControlCommand.TestCanvasFlags, ["set-slant", 125]);
      const left = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      if (num(left, "x0") >= num(left, "x1")) {
        throw new Error(`issue-6346: 125° grid did not lean left: ${left}`);
      }

      const clamped = String((await client.request(ControlCommand.TestCanvasFlags, ["set-slant", 5]))[1] ?? "");
      if (!/slant=15/.test(clamped)) {
        throw new Error(`issue-6346: slant 5 was not clamped to 15: ${clamped}`);
      }
      await client.request(ControlCommand.TestCanvasFlags, ["set-slant", 90]);

      const probe = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      const sx = num(probe, "sx");
      const sy = num(probe, "sy");
      const down = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "down", sx, sy]))[1] ?? "",
      );
      if (!/hit=1/.test(down)) {
        throw new Error(`issue-6346: width line was not draggable at ${sx},${sy}: ${down} probe ${probe}`);
      }
      const widened = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "up", sx + 80, sy]))[1] ?? "",
      );
      const width = num(widened, "width");
      if (!(width > 72)) {
        throw new Error(`issue-6346: dragging a vertical line did not widen the cell: ${widened}`);
      }
      if (num(widened, "height") !== width) {
        throw new Error(`issue-6346: width drag did not keep cells square: ${widened}`);
      }

      const resized = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      const hx = num(resized, "hx");
      const hy = num(resized, "hy");
      const hDown = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "down", hx, hy]))[1] ?? "",
      );
      if (!/hit=1/.test(hDown)) {
        throw new Error(`issue-6346: height line was not draggable at ${hx},${hy}: ${hDown} probe ${probe}`);
      }
      const moved = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "move", hx, hy + 80]))[1] ?? "",
      );
      if (num(moved, "height") === width) {
        throw new Error(`issue-6346: dragging a horizontal line did not change the cell height: ${moved}`);
      }
      if (num(moved, "width") !== num(moved, "height")) {
        throw new Error(`issue-6346: height drag did not keep cells square: ${moved}`);
      }
      await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "cancel"]);
      const back = String((await client.request(ControlCommand.TestCanvasFlags, []))[1] ?? "");
      if (num(back, "height") !== width) {
        throw new Error(`issue-6346: cancel did not restore the cell height: ${back}`);
      }
      if (num(back, "width") !== width) {
        throw new Error(`issue-6346: cancel changed the width: ${back}`);
      }

      await client.request(ControlCommand.TestCanvasFlags, ["set-slant", 55]);
      const leanProbe = String((await client.request(ControlCommand.TestCanvasFlags, ["probe-grid"]))[1] ?? "");
      const leanX = num(leanProbe, "sx");
      const leanY = num(leanProbe, "sy");
      const leanDown = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "down", leanX, leanY]))[1] ?? "",
      );
      if (!/hit=1/.test(leanDown)) {
        throw new Error(`issue-6346: slanted line was not draggable: ${leanDown} probe ${leanProbe}`);
      }
      const leanSize = String(
        (await client.request(ControlCommand.TestCanvasFlags, ["drag-grid", "up", leanX + 40, leanY]))[1] ?? "",
      );
      if (num(leanSize, "width") <= width || num(leanSize, "width") !== num(leanSize, "height")) {
        throw new Error(`issue-6346: slanted drag did not enlarge square cells: ${leanSize}`);
      }
    },
    ["-appdata", appdata, PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
