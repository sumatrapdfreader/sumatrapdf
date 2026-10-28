// #6328: the citation hover popup had a fixed pale yellow frame and a system
// gray border in every theme. In a dark theme its frame now takes the theme's
// HotBackgroundColor and its edge HotEdgeColor (Tokyo Night: #292e42, #7dcfff).
// A light theme keeps the yellow, with HotEdgeColor as the edge.
//
// A test's cursor can't hold a hover, so TestRefHover opens the popup. Page 1
// is one big link to page 2.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, runStandalone, tmpPath, USE_NG } from "./util";
import { captureWindowPixels, getClientRect, sleep } from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";

const cases = [
  { theme: "Tokyo Night", frame: "#292e42", edge: "#7dcfff" },
  { theme: "Light", frame: "#fffcc8", edge: "#808080" },
];

function makePdf(): string {
  const content1 = "BT /F1 24 Tf 72 700 Td (See reference [1]) Tj ET";
  const content2 = "BT /F1 10 Tf 72 600 Td (Reference entry) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> " +
      "/Contents 6 0 R /Annots [8 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${content1.length} >>\nstream\n${content1}\nendstream`,
    `<< /Length ${content2.length} >>\nstream\n${content2}\nendstream`,
    "<< /Type /Annot /Subtype /Link /Rect [0 0 612 792] /Border [0 0 0] /Dest [4 0 R /XYZ 72 600 null] >>",
  ]);
}

async function popupHwnd(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const res = await client.request(ControlCommand.TestRefHover, []);
    const m = /visible=1 hwnd=(-?\d+)/.exec(String(res[1] ?? ""));
    if (m) {
      return +m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6328: the popup did not open: ${res[1]}`);
    }
    await sleep(50);
  }
}

function rgbAt(px: { w: number; data: Uint8Array }, x: number, y: number): number[] {
  const i = (y * px.w + x) * 4;
  return [px.data[i + 2]!, px.data[i + 1]!, px.data[i]!];
}

function hex(c: number[]): string {
  return "#" + c.map((v) => v.toString(16).padStart(2, "0")).join("");
}

async function checkTheme(theme: string, wantFrame: string, wantEdge: string): Promise<void> {
  const dir = tmpPath("issue-6328");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "CitationHoverDelay = 100",
      `Theme = ${theme}`,
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const rc = getClientRect(canvas);
    const x = Math.floor((rc.right - rc.left) / 2);
    const y = Math.floor((rc.bottom - rc.top) / 3);
    const res = await client.request(ControlCommand.TestRefHover, ["show", x, y]);
    if (res[0] !== 0) {
      throw new Error(`issue-6328: could not show the popup: ${res[1]}`);
    }
    const hwnd = await popupHwnd(client);
    await sleep(200);
    const px = captureWindowPixels(hwnd, "window");
    if (!px) {
      throw new Error("issue-6328: could not capture the popup");
    }
    const edge = rgbAt(px, 0, 0);
    const fill = rgbAt(px, 2, 2);
    if (hex(edge) !== wantEdge || hex(fill) !== wantFrame) {
      throw new Error(
        `issue-6328: ${theme}: popup edge ${hex(edge)} / frame ${hex(fill)}, want ${wantEdge} / ${wantFrame}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("issue-6328: skipped for ng (its popup is drawn into the frame, not a window of its own)");
    return;
  }
  for (const c of cases) {
    await checkTheme(c.theme, c.frame, c.edge);
  }
  console.log("issue-6328: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
