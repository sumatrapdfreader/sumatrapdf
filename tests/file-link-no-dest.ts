// An ebook link to another local file without a destination opens that file and
// stops. It used to pass the source link to the target's ScrollTo; for an
// HTML target BrowserDocController::HandleLink asserted on its launchFile kind.
//
// Run: bun tests/file-link-no-dest.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, SLOW_BUILD_FACTOR, cmdId, makeZip, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import { getWindowText, sleep, waitForTopWindow } from "./winapi.ts";

const TARGET = "target.xhtml";

const XHTML = `<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>target</title></head>
<body><p>target</p></body></html>
`;

// one chapter whose only link is to TARGET, a file outside the archive: the
// engine makes it a launchFile link to <epub dir>/OEBPS/TARGET without a dest
function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>
<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const chapter =
    `<?xml version="1.0" encoding="utf-8"?>
` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>c1</title></head>` +
    `<body><h1><a href="${TARGET}">link to target</a></h1></body></html>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>
` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">file-link-no-dest</dc:identifier>` +
    `<dc:title>file link</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/></manifest>` +
    `<spine><itemref idref="c1"/></spine></package>`;
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(chapter) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
  ]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("file-link-no-dest");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "source.epub");
  writeFileSync(epub, makeEpub());
  mkdirSync(join(dir, "OEBPS"));
  writeFileSync(join(dir, "OEBPS", TARGET), XHTML);

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();

      // follow the link with keyboard link following
      const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      let dump = "";
      while (Date.now() < deadline) {
        sendCommandSync(frame, cmdId("CmdToggleKeyboardLinkFollowing"));
        const res = await client.request(ControlCommand.TestKeyboardLinkFollow, []);
        dump = String(res[1] ?? "");
        if (/active=1 .*count=1/.test(dump)) {
          break;
        }
        await client.request(ControlCommand.TestKeyboardLinkFollow, ["stop"]);
        await sleep(25);
      }
      if (!/active=1 .*count=1/.test(dump)) {
        throw new Error(`link following did not start:\n${dump}`);
      }
      const hint = / hint=([A-Z]+)$/m.exec(dump)![1]!;
      await client.request(ControlCommand.TestKeyboardLinkFollow, ["char", hint]);

      // the target opens; a debug report would have killed the process
      let title = "";
      while (Date.now() < deadline + 4000 * SLOW_BUILD_FACTOR) {
        title = getWindowText(frame);
        if (title.includes(TARGET)) {
          break;
        }
        await sleep(50);
      }
      await client.waitForRenderIdle();
      if (!title.includes(TARGET)) {
        throw new Error(`${TARGET} did not open, title: '${title}'`);
      }
    },
    [epub],
  );
  rmSync(dir, { recursive: true, force: true });
  console.log("file-link-no-dest: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
