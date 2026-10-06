import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { clientToScreen, packCoords, sendMessage, setCursorPos, sleep, WM_MOUSEMOVE } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const dates = ["2026-01-15T23:30:00Z", "2026-07-15T23:30:00Z"];

function localDate(iso: string): string {
  const d = new Date(iso);
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6288");
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "dates.pdf");
  const annots = dates.map((date, i) => {
    const x = 72 + i * 220;
    const stamp = date.replace(/[-:TZ]/g, "");
    return `<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [${x} 420 ${x + 120} 540] /C [1 0 0] /BS << /W 2 >> /M (D:${stamp}Z) >>`;
  });
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
      ...annots,
    ]),
    "latin1",
  );
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nCheckForUpdates = false\n",
  );
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    const initial = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    const squares = [...initial.matchAll(/type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g)];
    if (squares.length !== dates.length) {
      throw new Error(`issue-6288: missing annotations\n${initial}`);
    }
    for (const [i, square] of squares.entries()) {
      const x = +square[1]! + Math.floor(+square[3]! / 2);
      const y = +square[2]! + Math.floor(+square[4]! / 2);
      const screen = clientToScreen(canvas, x, y);
      setCursorPos(screen.x, screen.y);
      sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
      const expected = localDate(dates[i]!);
      const deadline = Date.now() + 5_000;
      let raw = "";
      for (;;) {
        raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
        const actual = /date=([^\r\n]+)/.exec(raw)?.[1]?.trim();
        if (actual === expected) {
          console.log(`issue-6288: ${dates[i]} -> ${actual}`);
          break;
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-6288: date=${actual}, want ${expected}\n${raw}`);
        }
        await sleep(60);
      }
    }
  } finally {
    await killAndWait(proc, client);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
