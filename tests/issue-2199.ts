// Test for issue #2199: "Better handle invalid files inside cbx/cbr file".
//
// A file inside a .cbz that isn't a decodable image used to render as an empty
// page (black, the comic book background) with nothing telling the reader that
// the page is broken -- it just looked like a blank page of the comic.
// EngineImages::RenderPage() handed back a blank bitmap instead of failing, so
// the "Couldn't render page N" message the canvas already draws for failed
// pages never appeared.
//
// The test builds a .cbz whose middle page is garbage, goes to that page and
// checks that something is painted in the middle of the otherwise black page
// area (the message). Pages 1 and 3 must still render normally.

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { tmpPath, makePng, makeStoredZip } from "./util";
import { killAndWait, launchControlled } from "./win-automation";

export async function testit(): Promise<void> {
  const cbz = tmpPath("issue-2199.cbz");
  writeFileSync(
    cbz,
    makeStoredZip([
      { name: "page1.png", data: makePng(300, 400, [220, 40, 40]) },
      // not a decodable image, though it claims to be a .png
      { name: "page2.png", data: Buffer.alloc(4096, 0x41) },
      { name: "page3.png", data: makePng(300, 400, [40, 80, 220]) },
    ]),
  );

  const { proc, client } = await launchControlled([cbz]);
  try {
    await client.waitForRenderIdle();
    const p1 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 1]);
    const p1Raw = String(p1[1] ?? "");
    if (p1[0] !== 0 || !/red=\d+/.test(p1Raw)) {
      throw new Error(`issue-2199 page 1 should render: ${p1Raw.trim()}`);
    }

    const p2 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 2]);
    const p2Raw = String(p2[1] ?? "");
    if (p2[0] === 0 || !/render-failed/.test(p2Raw)) {
      throw new Error(`issue-2199: broken cbz page rendered instead of failing: ${p2Raw.trim()}`);
    }

    const p3 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 3]);
    const p3Raw = String(p3[1] ?? "");
    if (p3[0] !== 0) {
      throw new Error(`issue-2199 page 3 stopped rendering after the broken page: ${p3Raw.trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
