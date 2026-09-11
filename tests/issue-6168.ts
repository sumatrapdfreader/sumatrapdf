// #6168: CmdDuplicateInNewTab must keep the current page (and not drift
// toward page 1 on repeated duplicates).
import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const kPages = 40;
const kPage = 31;
const kDuplicates = 7;

function makePdf(): Buffer {
  const fontObj = 3;
  const objects: string[] = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ];
  const kids: number[] = [];
  for (let page = 1; page <= kPages; page++) {
    const content = `BT /F1 24 Tf 72 720 Td (Page ${page}) Tj ET`;
    objects.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
    const contentId = objects.length;
    objects.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 ${fontObj} 0 R >> >> /Contents ${contentId} 0 R >>`,
    );
    kids.push(objects.length);
  }
  objects[1] = `<< /Type /Pages /Kids [${kids.map((n) => `${n} 0 R`).join(" ")}] /Count ${kPages} >>`;
  return Buffer.from(assemblePdf(objects), "latin1");
}

async function currentPage(client: { request: Function }): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6168: current tab: ${raw}`);
  }
  const m = / page=(\d+)$/.exec(raw);
  if (!m) {
    throw new Error(`issue-6168: could not parse: ${raw}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6168.pdf");
  writeFileSync(pdf, makePdf());
  const appdata = writeAppdata(
    "issue-6168-settings",
    `RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
DefaultZoom = fit page
DefaultDisplayMode = continuous
`,
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-zoom",
    "100",
    "-page",
    String(kPage),
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await pollUntil(
      () => currentPage(client),
      (p) => p === kPage,
      {
        timeoutMs: 10_000,
        error: (p) => `issue-6168: opened at page ${p}, want ${kPage}`,
      },
    );

    for (let i = 1; i <= kDuplicates; i++) {
      sendCommandSync(frame, cmdId("CmdDuplicateInNewTab"));
      await client.waitForRenderIdle();
      const page = await currentPage(client);
      if (page !== kPage) {
        throw new Error(`issue-6168: after duplicate ${i} page=${page}, want ${kPage}`);
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
