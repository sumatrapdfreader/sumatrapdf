// #6505: Convert to Images works for any rendered document, not only PDF.
// EPUB pages follow the current layout, XPS pages their fixed size. ng also
// used to show the command for OFD but not open its dialog.
//
// Run: bun tests/issue-6505.ts [--no-build] [-ng]

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, makeZip, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kTitle = "Convert to Images";
const kNgWindow = "converttoimages";
const kDpi = 150; // the dialog's default

const xpsNs = 'xmlns="http://schemas.microsoft.com/xps/2005/06"';

// one 2 x 1 inch page (XPS units are 1/96 inch)
function makeXps(): Buffer {
  const files: [string, string][] = [
    [
      "[Content_Types].xml",
      '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">' +
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' +
        '<Default Extension="fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/>' +
        '<Default Extension="fdoc" ContentType="application/vnd.ms-package.xps-fixeddocument+xml"/>' +
        '<Default Extension="fpage" ContentType="application/vnd.ms-package.xps-fixedpage+xml"/></Types>',
    ],
    [
      "_rels/.rels",
      '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' +
        '<Relationship Id="R1" Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" ' +
        'Target="/Seq.fdseq"/></Relationships>',
    ],
    ["Seq.fdseq", `<FixedDocumentSequence ${xpsNs}><DocumentReference Source="/Doc.fdoc"/></FixedDocumentSequence>`],
    ["Doc.fdoc", `<FixedDocument ${xpsNs}><PageContent Source="/1.fpage"/></FixedDocument>`],
    [
      "1.fpage",
      `<FixedPage ${xpsNs} Width="192" Height="96" xml:lang="en">` +
        '<Path Data="M 10,10 L 182,10 182,86 10,86 Z" Fill="#FF0000"/></FixedPage>',
    ],
  ];
  return makeZip(files.map(([name, xml]) => ({ name, data: Buffer.from(xml) })));
}

const ofdNs = 'xmlns="http://www.ofdspec.org/2016"';

// one 25 x 25 mm page
function makeOfd(): Buffer {
  const files: [string, string][] = [
    ["OFD.xml", `<OFD ${ofdNs}><DocBody><DocRoot>Doc/Document.xml</DocRoot></DocBody></OFD>`],
    [
      "Doc/Document.xml",
      `<Document ${ofdNs}><CommonData><PageArea><PhysicalBox>0 0 25 25</PhysicalBox></PageArea></CommonData>` +
        `<Pages><Page ID="1" BaseLoc="Page.xml"/></Pages></Document>`,
    ],
    [
      "Doc/Page.xml",
      `<Page ${ofdNs}><Content><Layer><PathObject ID="2" Boundary="5 5 15 15" Fill="true">` +
        `<AbbreviatedData>M 0 0 L 15 0 L 15 15 L 0 15 C</AbbreviatedData></PathObject></Layer></Content></Page>`,
    ],
  ];
  return makeZip(files.map(([name, xml]) => ({ name, data: Buffer.from(xml) })));
}

function pngSize(path: string): { w: number; h: number } {
  const b = readFileSync(path);
  return { w: b.readUInt32BE(16), h: b.readUInt32BE(20) };
}

async function waitFor<T>(what: string, get: () => Promise<T | null>, ms = 10000): Promise<T> {
  const deadline = Date.now() + ms;
  for (;;) {
    const v = await get();
    if (v !== null) {
      return v;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6505: ${what}`);
    }
    await sleep(50);
  }
}

// clicks the dialog's Convert button
async function clickConvert(client: ControlClient, pid: number): Promise<void> {
  if (USE_NG) {
    const tool = (args: (string | number)[]) => client.request(ControlCommand.TestToolWindow, args);
    const line = await waitFor("the dialog did not open", async () => {
      const layout = String((await tool(["layout", kNgWindow, "", 0, 0, 0, 0]))[1] ?? "");
      return layout.split("\n").find((l) => /^node .* label='Convert'/.test(l)) ?? null;
    });
    const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line)!;
    await tool(["input", kNgWindow, "click", +m[1]! + Math.floor(+m[3]! / 2), +m[2]! + Math.floor(+m[4]! / 2), 0, 0]);
    return;
  }
  const dlg = await waitFor("the dialog did not open", async () => {
    let found = 0;
    enumWindows((h) => {
      if (getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === kTitle) {
        found = h;
        return false;
      }
      return true;
    });
    return found || null;
  });
  const btn = await waitFor("no Convert button", async () => {
    const r = (await client.virtTree(getClassName(dlg))).find((n) => n.text === "Convert");
    return r ? r.rect : null;
  });
  const lp = packCoords(btn.x + Math.floor(btn.dx / 2), btn.y + Math.floor(btn.dy / 2));
  postMessage(dlg, WM_MOUSEMOVE, 0, lp);
  postMessage(dlg, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  postMessage(dlg, WM_LBUTTONUP, 0, lp);
}

// opens doc, converts all its pages and returns the first image's path
async function convert(doc: string): Promise<string> {
  const first = doc.replace(/\.[^.]+$/, "-1.png");
  const { proc, client, frame } = await launchControlled([doc]);
  try {
    await client.waitForRenderIdle();
    const vis = String(
      (await client.request(ControlCommand.TestCommandVisibility, ["CmdConvertPdfToImages", "palette"]))[1] ?? "",
    );
    if (!/vis=show/.test(vis)) {
      throw new Error(`issue-6505: Convert to Images not offered for ${doc}: ${vis}`);
    }
    if (USE_NG) {
      await client.request(ControlCommand.TestInvokeCommand, ["CmdConvertPdfToImages"]);
    } else {
      sendCommand(frame, cmdId("CmdConvertPdfToImages"));
    }
    await clickConvert(client, proc.pid);
    await waitFor(`${first} was not written`, async () => (existsSync(first) ? true : null));
  } finally {
    client.close();
    await killAndWait(proc);
  }
  return first;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6505");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  // XPS: a fixed 2 x 1 inch page
  const xps = join(dir, "doc.xps");
  writeFileSync(xps, makeXps());
  const xpsPng = pngSize(await convert(xps));
  if (xpsPng.w !== 2 * kDpi || xpsPng.h !== kDpi) {
    throw new Error(`issue-6505: XPS page should be ${2 * kDpi}x${kDpi} px, is ${xpsPng.w}x${xpsPng.h}`);
  }

  // EPUB: the laid-out page, whatever its size
  const epub = join(dir, "doc.epub");
  copyFileSync(join(ROOT, "tests", "issue-5846.epub"), epub);
  const epubPng = pngSize(await convert(epub));
  if (epubPng.w < 100 || epubPng.h < 100) {
    throw new Error(`issue-6505: EPUB page image is too small: ${epubPng.w}x${epubPng.h}`);
  }

  // OFD: was offered by ng's menu but its dialog didn't open
  const ofd = join(dir, "doc.ofd");
  writeFileSync(ofd, makeOfd());
  await convert(ofd);

  console.log("issue-6505: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
