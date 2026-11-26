import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

export async function testit(): Promise<void> {
  if (!IS_WIN) return;
  const dir = tmpPath("issue-6603-data");
  mkdirSync(dir, { recursive: true });
  const settings = join(dir, "SumatraPDF-settings.txt");
  async function measure(extension: string, formatSettings: string, ebookSettings = "", updateFont = false) {
    const file = join(dir, `layout.${extension}`);
    const section = extension === "md" ? "MarkdownUI" : "HtmlUI";
    writeFileSync(file, extension === "md" ? "First marker\nSecond marker\n" : "<p>First marker<br>Second marker</p>");
    writeFileSync(
      settings,
      `RestoreSession = false\n${section} [\n    UseFixedPageUI = true\n${formatSettings}\n]\n${ebookSettings}\n`,
    );
    return await withControlledSumatra(
      EXE,
      async (client) => {
        if (updateFont) {
          await client.waitForRenderIdle();
          const [code, raw] = await client.request(ControlCommand.TestSetSetting, [`${section}.FontSize`, "16"]);
          if (code !== 0) throw new Error(`Could not update ${section}.FontSize: ${raw}`);
        }
        await client.waitForRenderIdle();
        const [code, raw] = await client.request(ControlCommand.TestPageBoxes, [1]);
        const boxes = /engineMedia=([\d.,-]+) engineContent=([\d.,-]+)/.exec(String(raw));
        if (code !== 0 || !boxes) throw new Error(`Could not measure the ${extension} page: ${raw}`);
        return { media: boxes[1]!, content: boxes[2]!, height: Number(boxes[2]!.split(",")[3]) };
      },
      ["-appdata", dir, file],
    );
  }
  for (const extension of ["md", "html"]) {
    const normal = await measure(extension, "");
    const isolated = await measure(
      extension,
      "",
      "EBookUI [\n    FontSize = 25\n    LineSpacing = 3\n    Margin = 5 10\n    LayoutDx = 670\n    LayoutDy = 700\n    IgnoreDocumentCSS = true\n    CustomCSS = body { display: none !important; }\n]",
    );
    if (isolated.media !== normal.media || isolated.content !== normal.content) {
      throw new Error(`EBookUI changed the fixed-page ${extension} layout`);
    }
    const large = await measure(extension, "    FontSize = 16");
    if (large.height <= normal.height) throw new Error(`FontSize did not enlarge ${extension} text`);
    const updated = await measure(extension, "", "", true);
    if (updated.content !== large.content)
      throw new Error(`Changing FontSize did not refresh the open ${extension} file`);
    const lineBreaks = extension === "md" ? "    PreserveLineBreaks = true\n" : "";
    const multiline = extension === "md" ? await measure(extension, lineBreaks) : normal;
    if (extension === "md" && multiline.height <= normal.height) {
      throw new Error("PreserveLineBreaks did not break the fixed-page Markdown line");
    }
    const spaced = await measure(extension, lineBreaks + "    LineSpacing = 2");
    if (spaced.height <= multiline.height) throw new Error(`LineSpacing did not enlarge the ${extension} text block`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
