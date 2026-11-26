import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, USE_NG, runStandalone, tmpPath } from "./util.ts";
import { waitForFrame } from "./win-automation.ts";
import { clientToScreen, getWindowRect, getWorkArea } from "./winapi.ts";

async function checkFont(dir: string, extension: string, section: string): Promise<void> {
  const file = join(dir, `font.${extension}`);
  const log = join(dir, `${extension}.log`);
  const font = `Issue6602Missing${section}Font`;
  writeFileSync(file, extension === "md" ? "# Markdown\n\nRead this." : "<html><body>Read this.</body></html>");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `RestoreSession = false
EBookUI [
    FontName = Issue6602MissingEbookFont
]
${section} [
    UseFixedPageUI = true
    FontName = ${font}
]
`,
  );
  await withControlledSumatra(EXE, (client) => client.waitForRenderIdle(), [
    "-appdata",
    dir,
    "-log-to-file",
    log,
    file,
  ]);
  const output = readFileSync(log, "utf8");
  if (!output.includes(`ebook font '${font}' not found`) || output.includes("Issue6602MissingEbookFont")) {
    throw new Error(`${section} did not select its own font`);
  }
}

async function checkFrame(dir: string): Promise<void> {
  const work = getWorkArea();
  const x = work.left + 50;
  const dy = work.bottom - work.top + 4;
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `RestoreSession = false
UseTabs = true
ShowCaption = true
WindowState = 1
WindowPos = ${x} ${work.top} 600 ${dy}
`,
  );
  await withControlledSumatra(
    EXE,
    async (_client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      const rect = getWindowRect(frame);
      if (rect.top !== work.top || rect.bottom !== work.top + dy) {
        throw new Error(
          `Vertically snapped frame restored to ${rect.top}..${rect.bottom}, expected ${work.top}..${work.top + dy}`,
        );
      }
      if (clientToScreen(frame, 0, 0).y <= rect.top) {
        throw new Error("Restored caption did not leave a native top frame edge");
      }
    },
    ["-appdata", dir],
    { defaultWindowPos: true },
  );
}

export async function testit(): Promise<void> {
  if (!IS_WIN) return;
  const dir = tmpPath("issue-6602-data");
  mkdirSync(dir, { recursive: true });
  await checkFont(dir, "md", "MarkdownUI");
  await checkFont(dir, "html", "HtmlUI");
  if (!USE_NG) await checkFrame(dir);
}

if (import.meta.main) {
  await runStandalone(testit);
}
