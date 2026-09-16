// Comic books in single-page + fit-page used an estimated media box for a page
// whose size was not known yet, then measured it and re-laid out — a visible
// jump. Relayout now measures the shown page(s) first.
//
// Run: bun tests/comic-fit-page-relayout.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { ControlCommand } from "./control.ts";

function crc32(buf: Buffer): number {
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    let c = (crc ^ buf[n]!) & 0xff;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type: string, data: Buffer): Buffer {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, "latin1"), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

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

function makeZip(entries: { name: string; data: Buffer }[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "latin1");
    const crc = crc32(e.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(e.data.length, 18);
    lh.writeUInt32LE(e.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    locals.push(lh, name, e.data);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);
    ch.writeUInt16LE(20, 6);
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(e.data.length, 20);
    ch.writeUInt32LE(e.data.length, 24);
    ch.writeUInt16LE(name.length, 28);
    ch.writeUInt32LE(offset, 42);
    centrals.push(ch, name);
    offset += 30 + name.length + e.data.length;
  }
  const localBuf = Buffer.concat(locals);
  const centralBuf = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralBuf.length, 12);
  end.writeUInt32LE(localBuf.length, 16);
  return Buffer.concat([localBuf, centralBuf, end]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("comic-fit-page-relayout");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cbz = join(dir, "pages.cbz");
  const log = join(dir, "log.txt");
  writeFileSync(
    cbz,
    makeZip([
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
