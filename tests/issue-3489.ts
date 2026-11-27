import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { assemblePdf, cmdId, EXE, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { clickAt, findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { findTopWindow, getWindowRect } from "./winapi.ts";

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3489.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      "<< /Type /Annot /Subtype /FreeText /P 3 0 R /Rect [85 300 245 330] /Contents (Words here) /DA (/Helv 12 Tf 0 g) /BS << /W 0 >> >>",
    ]),
    "latin1",
  );
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const pid = proc.pid!;
      const frame = await waitForFrame(pid);
      const dump = async () => String((await client.request(ControlCommand.TestMarkupAnnots, []))[1]);
      const click = async (hwnd: number, x: number, y: number) => {
        if (USE_NG) {
          const result = await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
          if (result[0] !== 0 || !String(result[1]).startsWith("OK")) {
            throw new Error(`Click failed at ${x},${y}: ${result[1]}`);
          }
        } else {
          await clickAt(hwnd, x, y);
        }
      };
      const state = async () => {
        const raw = await dump();
        const m = /type=FreeText[^\n]*?rect=(\S+) screen=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
        if (!m) throw new Error(`FreeText annotation missing:\n${raw}`);
        return { rect: m[1]!.split(",").map(Number), screen: m.slice(2).map(Number) };
      };
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(false);
      sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
      const before = await state();
      const [x, y, w, h] = before.screen as [number, number, number, number];
      await click(USE_NG ? frame : findCanvas(frame), x + (w >> 1), y + (h >> 1));
      const toolbarState = async () => /annotEditToolbar visible=1[^\n]*/.exec(await dump())?.[0] ?? "";
      const sizeChipRe = /[=;]textSize:(-?\d+),(-?\d+),([1-9]\d*),([1-9]\d*)/;
      const toolbar = await pollUntil(toolbarState, (s) => sizeChipRe.test(s), {
        error: (s) => `Text Size chip missing:\n${s}`,
      });
      const placed = /placed=(-?\d+),(-?\d+)/.exec(toolbar);
      const chip = sizeChipRe.exec(toolbar)!;
      if (!placed) throw new Error(`Toolbar position missing:\n${toolbar}`);
      const row = USE_NG ? frame : findTopWindow(pid, "SumatraAnnotEditToolbar");
      const rowX = USE_NG ? 0 : +placed[1]!;
      const rowY = USE_NG ? 0 : +placed[2]!;
      await click(row, +chip[1]! - rowX + (+chip[3]! >> 1), +chip[2]! - rowY + (+chip[4]! >> 1));
      const sliderRe = /annotColorPopup visible=1[^\n]*thickness=(\d+):(-?\d+),(-?\d+),(\d+),(\d+)/;
      const sliderState = async () => /annotColorPopup [^\n]*/.exec(await dump())?.[0] ?? "";
      const slider = sliderRe.exec(
        await pollUntil(sliderState, (s) => sliderRe.test(s), { error: (s) => `Text Size slider missing:\n${s}` }),
      )!;
      const sx = +slider[2]! + +slider[4]! - 2;
      const sy = +slider[3]! + (+slider[5]! >> 1);
      if (USE_NG) {
        await click(frame, sx, sy);
      } else {
        const popup = findTopWindow(pid, "SumatraAnnotColorPopup");
        const r = getWindowRect(popup);
        await click(popup, sx - r.left, sy - r.top);
      }
      await pollUntil(sliderState, (s) => Number(sliderRe.exec(s)?.[1]) >= 70, {
        error: (s) => `Text Size slider did not reach at least 70 pt:\n${s}`,
      });
      const after = await state();
      if (after.rect.slice(0, 3).join() !== before.rect.slice(0, 3).join() || after.rect[3]! < 2 * 70) {
        throw new Error(`Increasing font size must grow the annotation downward: ${before.rect} -> ${after.rect}`);
      }
      await client.request(ControlCommand.TestInvokeCommand, ["CmdUndo"]);
      await client.waitForRenderIdle();
      const undone = await state();
      if (undone.rect.join() !== before.rect.join()) {
        throw new Error(`Undo must restore the original box in one step: ${before.rect} -> ${undone.rect}`);
      }
    },
    ["-view", "single page", "-zoom", "fit page", pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
