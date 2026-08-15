// #3472: with Fit Width, a reflowable ebook page was always taller than the
// window, so reading meant scrolling inside a page and then turning it -- two
// levels of scrolling. A reflow document is laid out once into a fixed-size
// page (A5 by default) and Fit Width only scales that page, so on any window
// whose shape isn't A5 the height overflows. The layout height is now derived
// from the window, so one page is one screen.
//
// GUI automation rather than -dbg-control: the whole point is what the layout
// does with the real window size.

import { findCanvas, launchControlled, killAndWait } from "./win-automation.ts";
import { getClientRect, getScrollInfo, setForegroundWindow, SB_VERT } from "./winapi.ts";
import { runStandalone } from "./util.ts";

const EPUB = "tests/issue-5846.epub";

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit width", EPUB]);
  try {
    await client.waitForRenderIdle();
    setForegroundWindow(frame);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-3472: no canvas");
    }
    const cr = getClientRect(canvas);
    const si = getScrollInfo(canvas, SB_VERT);
    // in single-page view the vertical scroll range is the page; when the page
    // fits, the range is the viewport and there is nothing to scroll
    const overflow = si.max + 1 - si.page;
    if (si.page < 100) {
      throw new Error(`issue-3472: document didn't load (scroll info ${JSON.stringify(si)})`);
    }
    if (overflow > 8) {
      throw new Error(
        `issue-3472: Fit Width page overflows the window by ${overflow}px ` +
          `(canvas ${cr.right}x${cr.bottom}, scroll ${JSON.stringify(si)})`,
      );
    }
    console.log(`issue-3472: canvas ${cr.right}x${cr.bottom}, Fit Width page overflow ${overflow}px`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
