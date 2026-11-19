// discussion #6543: FixedPageUI.WindowBgCol colors the canvas around the pages
// of every fixed-page document (DjVu, XPS), not only PDF.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control";
import { makeTextPdf } from "./issue-4684";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { captureWindowPixels, sleep, waitForTopWindow } from "./winapi";

const kDjvu = join(import.meta.dir, "issue-6312.djvu");
// nothing in the UI is magenta, so every such pixel is canvas background
const kSettings = `FixedPageUI [
    WindowBgCol = #ff00ff
]
`;

function countMagenta(frame: number): number {
  const px = captureWindowPixels(frame);
  if (!px) throw new Error("discussion-6543: can't capture the window");
  let n = 0;
  for (let i = 0; i < px.data.length; i += 4) {
    // BGRA
    if (px.data[i] === 0xff && px.data[i + 1] === 0 && px.data[i + 2] === 0xff) n++;
  }
  return n;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("discussion-6543");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), kSettings);

  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makeTextPdf());
  for (const doc of [kDjvu, pdf]) {
    await checkCanvas(dir, doc);
  }
}

async function checkCanvas(dir: string, doc: string): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("discussion-6543: no frame window");
      await client.setNotificationsEnabled(false);
      const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
      let n = 0;
      while (Date.now() < deadline) {
        await client.waitForRenderIdle();
        n = countMagenta(frame);
        if (n > 1000) return;
        await sleep(100);
      }
      throw new Error(`discussion-6543: ${doc}: canvas isn't FixedPageUI.WindowBgCol (${n} magenta pixels)`);
    },
    ["-appdata", dir, doc],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
