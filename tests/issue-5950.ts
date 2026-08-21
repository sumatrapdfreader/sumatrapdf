// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5950
//
// Document Properties for a standalone PNG must show Image Size (and DPI when
// known) and must not show Reading Direction. Image size used to be gated on
// EXIF parse, so a typical PNG showed neither.
//
// The fixture is generated here (IHDR + pHYs, no EXIF).
// Run:  bun tests/issue-5950.ts [--no-build]

import { writeFileSync } from "node:fs";
import { crc32, deflateSync } from "node:zlib";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";

const IMG_W = 12;
const IMG_H = 8;
const IMG_DPI = 300;

function be32(n: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeUInt32BE(n >>> 0);
  return b;
}

function pngChunk(type: string, data: Buffer): Buffer {
  const t = Buffer.from(type, "ascii");
  const crcBuf = Buffer.alloc(4);
  crcBuf.writeUInt32BE(crc32(Buffer.concat([t, data])) >>> 0);
  return Buffer.concat([be32(data.length), t, data, crcBuf]);
}

// 8-bit RGB PNG with optional pHYs (pixels/meter). No EXIF.
function makePng(w: number, h: number, dpi: number): Buffer {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  const chunks = [pngChunk("IHDR", ihdr)];
  if (dpi > 0) {
    const ppm = Math.round(dpi / 0.0254);
    const phys = Buffer.alloc(9);
    phys.writeUInt32BE(ppm, 0);
    phys.writeUInt32BE(ppm, 4);
    phys[8] = 1;
    chunks.push(pngChunk("pHYs", phys));
  }
  const stride = 1 + w * 3;
  const raw = Buffer.alloc(h * stride);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const o = y * stride + 1 + x * 3;
      raw[o] = 200;
      raw[o + 1] = 40;
      raw[o + 2] = 40;
    }
  }
  chunks.push(pngChunk("IDAT", deflateSync(raw)));
  chunks.push(pngChunk("IEND", Buffer.alloc(0)));
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), ...chunks]);
}

function findFirstEdit(parent: number): number {
  let found = 0;
  enumChildWindows(parent, (hwnd) => {
    if (getClassName(hwnd) === "Edit") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForPropertiesEdit(pid: number, timeoutMs = 8000): Promise<{ props: number; edit: number }> {
  const deadline = Date.now() + timeoutMs;
  let lastProps = 0;
  while (Date.now() < deadline) {
    let props = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) !== pid) {
        return true;
      }
      if (getWindowText(hwnd).includes("Document Properties")) {
        props = hwnd;
        return false;
      }
      return true;
    });
    if (props) {
      lastProps = props;
      const edit = findFirstEdit(props);
      if (edit) {
        return { props, edit };
      }
    }
    await sleep(100);
  }
  return { props: lastProps, edit: 0 };
}

async function propertiesText(frame: number, pid: number): Promise<string> {
  sendCommandSync(frame, cmdId("CmdProperties"));
  const { props, edit } = await waitForPropertiesEdit(pid);
  if (!props) {
    throw new Error("issue-5950: Document Properties window did not open");
  }
  if (!edit) {
    throw new Error("issue-5950: no Edit control in Document Properties");
  }
  let text = "";
  for (let i = 0; i < 20; i++) {
    text = getControlText(edit);
    if (text.includes("File:") || text.includes("Image Size:")) {
      break;
    }
    await sleep(100);
  }
  postMessage(props, WM_CLOSE, 0, 0);
  return text;
}

export async function testit(): Promise<void> {
  const pngPath = tmpPath("issue-5950.png");
  writeFileSync(pngPath, makePng(IMG_W, IMG_H, IMG_DPI));

  const { proc, client, frame } = await launchControlled([pngPath]);
  try {
    await client.waitForRenderIdle();
    const text = await propertiesText(frame, proc.pid!);
    console.log(`issue-5950 properties text (${text.length} chars):\n${text}`);

    if (!/Image Size:\s*12 x 8/.test(text)) {
      throw new Error(`issue-5950: missing Image Size 12 x 8:\n${text}`);
    }
    if (!/DPI:\s*300\b/.test(text)) {
      throw new Error(`issue-5950: missing DPI 300:\n${text}`);
    }
    if (/Reading Direction:/.test(text)) {
      throw new Error(`issue-5950: Reading Direction should not show for a PNG:\n${text}`);
    }
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
