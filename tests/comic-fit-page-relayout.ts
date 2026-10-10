// Comic books in single-page + fit-page used an estimated media box for a page
// whose size was not known yet, then measured it and re-laid out — a visible
// jump. Relayout now measures the shown page(s) first.
//
// Run: bun tests/comic-fit-page-relayout.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath, pngChunk, makeStoredZip } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { ControlCommand } from "./control.ts";

function makePng(w: number, h: number): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("comic-fit-page-relayout");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cbz = join(dir, "pages.cbz");
  const log = join(dir, "log.txt");
  writeFileSync(
    cbz,
    makeStoredZip([
      { name: "001.png", data: makePng(80, 120) },
      { name: "002.png", data: makePng(80, 120) },
      { name: "003.png", data: makePng(240, 80) },
    ]),
  );
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    [
      "ReuseInstance = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "DefaultDisplayMode = single page",
      "DefaultZoom = fit page",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-log-to-file", log, cbz]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdZoomFitPageAndSinglePage"));
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await client.waitForRenderIdle();
    const pageRes = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    const page = /OK page=(\d+)/.exec(String(pageRes[1] ?? ""));
    if (!page || page[1] !== "3") {
      throw new Error(`comic-fit-page-relayout: expected page 3, got ${String(pageRes[1] ?? "").trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const txt = readFileSync(log, "utf8");
  if (txt.includes("re-layout: measured")) {
    throw new Error(`comic-fit-page-relayout: unexpected media-box re-layout:\n${txt}`);
  }
  console.log("comic-fit-page-relayout: no estimated-size re-layout on page 1 or 3");
}

if (import.meta.main) {
  await runStandalone(testit);
}
