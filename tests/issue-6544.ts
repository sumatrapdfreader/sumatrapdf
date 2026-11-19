// #6544: SVG clip-path was ignored, so plots drew their data outside the axes.
// issue-6544.svg clips a magenta rect to half its width; an unclipped green rect
// has half that clipped area. Ratio 2 = clipped, 3 = clip ignored, 1 = clip in
// the wrong user space.
import { join } from "node:path";
import { withControlledSumatra } from "./control";
import { EXE, runStandalone, SLOW_BUILD_FACTOR } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { captureWindowPixels, sleep, waitForTopWindow } from "./winapi";

const kSvg = join(import.meta.dir, "issue-6544.svg");

function countColors(frame: number): { magenta: number; green: number } {
  const px = captureWindowPixels(frame);
  if (!px) throw new Error("issue-6544: can't capture the window");
  let magenta = 0;
  let green = 0;
  for (let i = 0; i < px.data.length; i += 4) {
    const [b, g, r] = [px.data[i], px.data[i + 1], px.data[i + 2]];
    if (r === 0xff && g === 0 && b === 0xff) magenta++;
    if (r === 0 && g === 0xff && b === 0) green++;
  }
  return { magenta, green };
}

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6544: no frame window");
      await client.setNotificationsEnabled(false);
      const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
      let c = { magenta: 0, green: 0 };
      while (Date.now() < deadline) {
        await client.waitForRenderIdle();
        c = countColors(frame);
        if (c.green > 1000) break;
        await sleep(100);
      }
      const ratio = c.magenta / c.green;
      if (!(ratio > 1.7 && ratio < 2.3)) {
        throw new Error(`issue-6544: magenta/green = ${ratio.toFixed(2)} (${c.magenta}/${c.green}), want 2`);
      }
    },
    [kSvg],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
