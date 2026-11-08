// CmdViewAsHex opens a document in a new hex view tab, and turns the tab of a
// file that failed to load into a hex view in place.

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { getWindowText } from "./winapi.ts";
import { killAndWait, launchControlled, waitForTitle } from "./win-automation.ts";

async function viewAsHex(file: string, expectTitle: string): Promise<void> {
  const { proc, client, frame } = await launchControlled([file]);
  try {
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdViewAsHex"]);
    const title = await waitForTitle(frame, (t) => t.includes(expectTitle)).catch(() => getWindowText(frame));
    if (!title.includes(expectTitle)) {
      throw new Error(`hex-view: title '${title}', expected '${expectTitle}'`);
    }
  } finally {
    await client.quit().catch(() => {});
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await viewAsHex("tests/issue-1189.pdf", "issue-1189.pdf (hex)");

  // not a PDF: the load fails and the error tab becomes the hex view
  const bad = tmpPath("hex-view-bad.pdf");
  const bytes = new Uint8Array(4096);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = (i * 31 + (i >> 8)) & 0xff;
  }
  writeFileSync(bad, bytes);
  await viewAsHex(bad, "hex-view-bad.pdf (hex)");
  console.log("hex-view: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
