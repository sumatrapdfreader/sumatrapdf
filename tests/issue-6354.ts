// PreserveLineBreaks keeps source newlines in the Markdown browser view.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const TARGET_DEST_NO = 3;
const MIN_EXTRA_SCROLL_Y = 200;
const SOURCE_LINES = 20;
const MARKDOWN =
  "# Start\n\n" +
  Array.from({ length: SOURCE_LINES }, (_, i) => `Line ${i + 1}`).join("\n") +
  "\n\n## Target\n\n" +
  Array.from({ length: 40 }, (_, i) => `Paragraph ${i + 1}.`).join("\n\n");

async function requestReady(client: ControlClient, destNo: number, minScrollY: number): Promise<string> {
  const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [destNo, minScrollY]);
    const raw = String(res[1] ?? "");
    if (res[0] === 0) return raw;
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`issue-6354: Markdown browser not ready: ${raw}`);
    }
    await sleep(40);
  }
}

async function targetY(mode: "default" | "false" | "true"): Promise<number> {
  const dir = tmpPath(`issue-6354-${mode}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const md = join(dir, "sample.md");
  writeFileSync(md, MARKDOWN);
  const value = mode === "default" ? "" : `PreserveLineBreaks = ${mode}\n`;
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\n` +
      `MarkdownUI [\nUseFixedPageUI = false\n${value}]\n`,
  );
  const { proc, client } = await launchControlled(["-appdata", dir, "-window-pos", "820x600@20x20", md]);
  try {
    await requestReady(client, 0, 0);
    const navigation = await requestReady(client, TARGET_DEST_NO, 0);
    if (!navigation.includes("#target")) {
      throw new Error(`issue-6354: wrong target heading: ${navigation}`);
    }
    const raw = await requestReady(client, 0, 1);
    const match = /scrollY=(\d+)/.exec(raw);
    if (!match) throw new Error(`issue-6354: no scroll position: ${raw}`);
    return Number(match[1]);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const normal = await targetY("default");
  const disabled = await targetY("false");
  const preserved = await targetY("true");
  if (Math.abs(normal - disabled) > 1) {
    throw new Error(`issue-6354: default is not standard Markdown (${normal} vs ${disabled})`);
  }
  if (preserved < disabled + MIN_EXTRA_SCROLL_Y) {
    throw new Error(`issue-6354: source lines were not preserved (${disabled} vs ${preserved})`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
