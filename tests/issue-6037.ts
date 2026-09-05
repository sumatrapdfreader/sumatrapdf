// #6037: creating a stamp from the command palette / WM_COMMAND must add it
// immediately (SetSelectedAnnotation used to only ScheduleRepaint).
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const created = await client.request(ControlCommand.TestInvokeCommand, ["CmdCreateAnnotStamp", 80, 80]);
    if (created[0] !== 0) {
      throw new Error(`issue-6037: create stamp: ${String(created[1] ?? "")}`);
    }
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6037: markup: ${raw.trim()}`);
    }
    if (!/type=Stamp/.test(raw)) {
      throw new Error(`issue-6037: stamp not created after CmdCreateAnnotStamp:\n${raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
