// A notification replaces the previous one of its group, as in orig, except:
// ad-hoc notifications stack, and a tab's notification leaves another tab's
// alone (multi-file load keeps a per-tab "Show Errors"). ng removed both.
//
// ng only: orig's rules are in src/Notifications.cpp NotifsAdd.
// Run: bun tests/notif-groups.ts [--no-build] -ng

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

async function notif(client: ControlClient, op: number): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, ["notif", op]))[1] ?? "");
  const m = /\bcount=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`notif-groups: bad reply: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP notif-groups: ng only (run with -ng)");
    return;
  }
  const a = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const b = join(ROOT, "docs", "test", "links.pdf");
  const { proc, client } = await launchControlled([a, b]);
  try {
    await client.waitForRenderIdle(30000);
    await client.setNotificationsEnabled(true);
    const base = await notif(client, 0);
    // two ad-hoc notifications stack
    await notif(client, 1);
    const adHoc = await notif(client, 1);
    if (adHoc !== base + 2) {
      throw new Error(`notif-groups: ${adHoc - base} ad-hoc notifications after showing 2, want 2`);
    }
    // a group's notification for tab 1 leaves tab 0's alone; a second one for tab 0 replaces it
    await notif(client, 2);
    const both = await notif(client, 3);
    if (both !== adHoc + 2) {
      throw new Error(`notif-groups: ${both - adHoc} grouped notifications for 2 tabs, want 2`);
    }
    const replaced = await notif(client, 2);
    if (replaced !== both) {
      throw new Error(`notif-groups: showing tab 0's again gave ${replaced - adHoc}, want it replaced (2)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("notif-groups: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
