// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6495
//
// A toolbar button made from a Shortcuts entry (ToolbarText) was greyed out on
// the Home page even for a command that works without a document, like
// CmdTabGroupRestore. The entry is a clone with its own command id, and the
// "works without a document" check looked at that id, not the command's.
//
// Run:  bun tests/issue-6495.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kSettings = `RestoreSession = false
CheckForUpdates = false
Shortcuts [
    [
        Cmd = CmdTabGroupRestore
        ToolbarText = Tabs-Get
    ]
    [
        Cmd = CmdTabGroupSave
        ToolbarText = Tabs-Save
    ]
    [
        Cmd = CmdInvertColors
        ToolbarText = Invert
    ]
]
`;

// "button-enabled idx=3 enabled=1 text=Tabs-Get" => { "Tabs-Get": true }
async function buttonsEnabled(client: ControlClient): Promise<Map<string, boolean>> {
  const [, raw] = await client.request(ControlCommand.TestToolbarButtons, []);
  const res = new Map<string, boolean>();
  for (const line of String(raw ?? "").split("\n")) {
    const m = /^button-enabled idx=\d+ enabled=(\d) text=(.*)$/.exec(line.trim());
    if (m) {
      res.set(m[2], m[1] === "1");
    }
  }
  return res;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6495");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), kSettings);

  // no document: the Home page
  await withControlledSumatra(
    EXE,
    async (client) => {
      // restore, invert: work without a document; save: needs open documents
      const want: [string, boolean][] = [
        ["Tabs-Get", true],
        ["Invert", true],
        ["Tabs-Save", false],
      ];
      let got = new Map<string, boolean>();
      for (let i = 0; i < 50; i++) {
        got = await buttonsEnabled(client);
        if (want.every(([name, on]) => got.get(name) === on)) {
          return;
        }
        await sleep(100);
      }
      const desc = want.map(([name, on]) => `${name}: got ${got.get(name)}, want ${on}`).join("; ");
      throw new Error(`issue-6495: Home page toolbar: ${desc}`);
    },
    ["-appdata", dir],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
