// Advanced Settings: with a long translation of "Unsaved changes: 1" (German)
// the footer pushed Cancel and Save out of the window (#6581). They must stay
// inside it at the default width and at the narrowest the window allows.
//
// Run: bun tests/issue-6581.ts [--no-build] [-ng]
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

type Footer = { clientDx: number; cancel: [number, number]; save: [number, number]; hints: number; raw: string };

async function adv(client: ControlClient, action: string, arg = 0): Promise<string> {
  const deadline = Date.now() + 15000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, arg]);
    const out = String(res[1] ?? "");
    if (res[0] === 0) {
      return out;
    }
    if (!out.startsWith("NOTREADY") || Date.now() > deadline) {
      throw new Error(`issue-6581: TestAdvSettingsRows(${action}) failed: ${out.trim()}`);
    }
    await sleep(100);
  }
}

function parseFooter(out: string): Footer {
  const m = /clientDx=(\d+) cancel=(-?\d+),(-?\d+) save=(-?\d+),(-?\d+) hints=(\d)/.exec(out);
  if (!m) {
    throw new Error(`issue-6581: bad footer report: ${out.trim()}`);
  }
  return { clientDx: +m[1]!, cancel: [+m[2]!, +m[3]!], save: [+m[4]!, +m[5]!], hints: +m[6]!, raw: out.trim() };
}

// the layout settles over a frame or two after a change. A resize is applied
// before this returns, but the report is still the previous frame, so a
// requested width has to come back the same twice.
async function footerFits(client: ControlClient, what: string, resizeTo = 0): Promise<Footer> {
  let f = parseFooter(await adv(client, "footer", resizeTo));
  const deadline = Date.now() + 5000;
  let prevDx = -1;
  let stable = 0;
  for (;;) {
    const fits = f.save[1] > f.save[0] && f.save[1] <= f.clientDx && f.cancel[1] <= f.save[0];
    stable = f.clientDx === prevDx ? stable + 1 : 0;
    if (fits && (resizeTo <= 0 || stable >= 1)) {
      return f;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6581: ${what}: Cancel / Save don't fit the window: ${f.raw}`);
    }
    await sleep(100);
    prevDx = f.clientDx;
    f = parseFooter(await adv(client, "footer"));
  }
}

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-lang", "de"]);
  try {
    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    await adv(client, "names");
    // one edit shows "Nicht gespeicherte Änderungen: 1" in the footer
    await adv(client, "toggle", 0);
    const atDefault = await footerFits(client, "default width");
    console.log(`issue-6581: default: ${atDefault.raw}`);
    const narrow = await footerFits(client, "narrowest width", 200);
    console.log(`issue-6581: narrowest: ${narrow.raw}`);
    if (narrow.hints !== 0) {
      throw new Error(`issue-6581: the key hints must give way at the narrowest width: ${narrow.raw}`);
    }
    // wide again: the key hints come back
    const wide = await footerFits(client, "wide again", 1400);
    const deadline = Date.now() + 5000;
    let f = wide;
    while (f.hints !== 1 && Date.now() < deadline) {
      await sleep(100);
      f = parseFooter(await adv(client, "footer"));
    }
    if (f.hints !== 1) {
      throw new Error(`issue-6581: the key hints didn't come back when wide: ${f.raw}`);
    }
    console.log(`issue-6581: wide: ${f.raw}`);
    await adv(client, "toggle", 0);
  } finally {
    await client.quit().catch(() => {});
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
