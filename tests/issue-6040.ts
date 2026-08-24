// #6040: image annotations created from the context menu must use the menu's
// canvas click point, like every other annotation, rather than wherever the
// cursor ended up after picking an image or closing the menu.

import { runStandalone } from "./util";
import { ControlCommand } from "./control";
import { killAndWait, launchControlled } from "./win-automation";

export async function testit(): Promise<void> {
  const { proc, client } = await launchControlled([]);
  try {
    const cases: [string, boolean][] = [
      ["CmdCreateAnnotFreeText", true],
      ["CmdCreateAnnotImageFromClipboard", true],
      ["CmdInsertImage", true],
      ["CmdOpenFile", false],
    ];
    for (const [name, expected] of cases) {
      const res = await client.request(ControlCommand.TestContextMenuPoint, [name]);
      if (res[0] !== 0) {
        throw new Error(`issue-6040: ${name}: ${String(res[1] ?? res[0])}`);
      }
      const actual = String(res[1] ?? "") === "1";
      if (actual !== expected) {
        throw new Error(`issue-6040: ${name}: expected context-menu point=${expected}, got ${actual}`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
