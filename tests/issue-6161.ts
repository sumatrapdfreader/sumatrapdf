// Fuzzed image files whose internal offsets sit near INT_MAX crashed the app
// while sniffing dimensions: the bounds checks computed `off + n`, which
// overflows, so a wild read got through. Opening each must not crash.

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { sendCopyDataW } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;

// prettier-ignore
const repros: { name: string; bytes: number[] }[] = [
  // TIFF (big-endian), first IFD at offset 0x7FFFFFFF
  { name: "huge-ifd-off.tif", bytes: [0x4d, 0x4d, 0x00, 0x2a, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x00] },
  // TIFF with an ImageWidth entry whose 5 values live at offset 0x7FFFFFFF
  { name: "huge-val-off.tif", bytes: [
      0x4d, 0x4d, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x7f, 0xff, 0xff, 0xff] },
  // JPEG XR (little-endian TIFF container), first IFD at offset 0x7FFFFFFF
  { name: "huge-ifd-off.jxr", bytes: [0x49, 0x49, 0xbc, 0x00, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00] },
  // ICO whose only directory entry points its image data at offset 0x7FFFFFFF
  { name: "huge-img-off.ico", bytes: [
      0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f] },
];

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6161");
  mkdirSync(dir, { recursive: true });
  const paths = repros.map(({ name, bytes }) => {
    const path = join(dir, name);
    writeFileSync(path, new Uint8Array(bytes));
    return path;
  });

  const { proc, client, frame } = await launchControlled([paths[0]!]);
  try {
    // the app died while opening each file, so answering a ping is the test
    await client.request(ControlCommand.Ping, []);
    for (let i = 1; i < paths.length; i++) {
      if (!sendCopyDataW(frame, kCopyDataDdeW, `[Open("${paths[i]}")]`)) {
        throw new Error(`issue-6161: could not open ${paths[i]}`);
      }
      await client.request(ControlCommand.Ping, []);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6161: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
