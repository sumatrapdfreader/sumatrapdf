// #6241: Zoom: Fit Visible fits the width of the page content (blank margins
// cropped, 2pt of them kept as padding) to the window, like Foxit's Fit
// Visible. On a page whose content spans a quarter of the page width the real
// zoom must be about 4x Fit Width.
//
// Run: bun tests/issue-6241.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util.ts";

const PAGE_W = 612;
const CONTENT_W = 153; // a quarter of the page
const PAD = 2; // kFitVisiblePadding, on both sides

function makePdf(): string {
  const content = `0 g 200 300 ${CONTENT_W} 300 re f`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_W} 792] /Contents 4 0 R >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ]);
}

async function zoomState(client: ControlClient): Promise<{ zoom: string; zoomReal: number }> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "").trim();
  const m = /zoom=(.+)$/.exec(raw);
  const rawReal = String((await client.request(ControlCommand.TestDisplayMode, ["zoom-real"]))[1] ?? "").trim();
  const mr = /zoomReal=([\d.]+)/.exec(rawReal);
  if (!m || !mr) {
    throw new Error(`issue-6241: could not parse '${raw}' / '${rawReal}'`);
  }
  return { zoom: m[1]!, zoomReal: parseFloat(mr[1]!) };
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6241.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      await client.waitForRenderIdle();
      let s = await zoomState(client);
      if (s.zoom !== "fit visible") {
        throw new Error(`issue-6241: -zoom "fit visible" gave '${s.zoom}'`);
      }
      const fitVisible = s.zoomReal;

      await client.request(ControlCommand.TestInvokeCommand, ["CmdZoomFitWidth"]);
      await client.waitForRenderIdle();
      s = await zoomState(client);
      const fitWidth = s.zoomReal;

      // the content box is detected from the rendered page, so allow it a little
      // slack around the drawn rectangle
      const want = PAGE_W / (CONTENT_W + 2 * PAD);
      const got = fitVisible / fitWidth;
      if (Math.abs(got - want) > want * 0.05) {
        throw new Error(`issue-6241: fit visible / fit width = ${got.toFixed(2)}, want ~${want.toFixed(2)}`);
      }

      // z cycles through it after Fit Content
      await client.request(ControlCommand.TestInvokeCommand, ["CmdZoomFitContent"]);
      await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleZoom"]);
      s = await zoomState(client);
      if (s.zoom !== "fit visible") {
        throw new Error(`issue-6241: Toggle Zoom after Fit Content gave '${s.zoom}'`);
      }
      console.log(`issue-6241: fit visible is ${got.toFixed(2)}x fit width ✓`);
    },
    ["-zoom", "fit visible", pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
