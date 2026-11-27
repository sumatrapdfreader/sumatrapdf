// "Current page" in the Windows 11 print dialog printed page 1. The dialog
// reports it as the number of the page its preview shows, and we renumbered
// the preview's pages to the picked range: after picking the page it was the
// preview's only one, page 1, which is what the dialog reported from then on.
// The preview must keep the document's page numbers.
//
// The dialog is the system's, in another process: it is driven through
// UI Automation from Windows PowerShell.
// Run: bun tests/issue-6609.ts [--no-build]

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename } from "node:path";
import { IS_WIN } from "./host.ts";
import { USE_NG, cmdId, makePdf, pollUntil, runPowerShell, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kPageCount = 6;
const kStartPage = 4;
// the items of the dialog's "Pages" list, whose names are localized
const kItemCurrentPage = 1;
const kItemCustomRange = 2;

type Pager = { page: number; total: number };
type Selection = { pages: number; first: number };

// Runs `action` on the print dialog of the app `pid` printing `name`, then
// reports the preview's "page / total".
function printDialog(pid: number, name: string, action: string): Pager {
  const script = `
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
$S = [System.Windows.Automation.TreeScope]
$T = [System.Windows.Automation.Condition]::TrueCondition
function ById($el, $id) {
  $cond = New-Object System.Windows.Automation.PropertyCondition($A::AutomationIdProperty, $id)
  return $el.FindFirst($S::Descendants, $cond)
}
$out = @{ error = 'no print dialog' }
try {
  $dialog = $null
  for ($i = 0; $i -lt 40 -and -not $dialog; $i++) {
    foreach ($w in $A::RootElement.FindAll($S::Children, $T)) {
      if ($w.Current.ProcessId -ne ${pid} -and $w.Current.Name -like '*${name}*') { $dialog = $w }
    }
    if (-not $dialog) { Start-Sleep -Milliseconds 250 }
  }
  if ($dialog) {
    $action = '${action}'
    $pages = ById $dialog 'com.microsoft.JobCustomPageRange_ItemList'
    if ($action -like 'pick *') {
      $expand = $pages.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
      $expand.Expand()
      Start-Sleep -Milliseconds 700
      $isItem = New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, [System.Windows.Automation.ControlType]::ListItem)
      $items = $pages.FindAll($S::Descendants, $isItem)
      $item = $items[[int]$action.Substring(5)]
      $item.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select()
      Start-Sleep -Milliseconds 300
      try { $expand.Collapse() } catch {}
    }
    if ($action -like 'range *') {
      $edit = ById $dialog 'com.microsoft.JobCustomPageRange_ValueText'
      $edit.SetFocus()
      $edit.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($action.Substring(6))
      Start-Sleep -Milliseconds 300
      # the range is taken when the box loses the focus
      (ById $dialog 'PrintButton').SetFocus()
    }
    if ($action -eq 'close') {
      (ById $dialog 'CloseButton').GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
      $out = @{ page = 0; total = 0 }
    } else {
      $page = (ById $dialog 'CurrentPageNumber').Current.Name
      $total = (ById $dialog 'TotalPageNumber').Current.Name
      $out = @{ page = [int]$page; total = [int]$total }
    }
  }
} catch {
  $out = @{ error = $_.Exception.Message }
}
$out | ConvertTo-Json -Compress
`;
  const r = runPowerShell(script);
  const line = r.out.trim().split(/\r?\n/).pop() ?? "";
  let res: any;
  try {
    res = JSON.parse(line);
  } catch {
    throw new Error(`issue-6609: bad PowerShell output:\n${r.out}`);
  }
  if (res.error) {
    throw new Error(`issue-6609: print dialog, ${action || "read"}: ${res.error}`);
  }
  return res;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("SKIP issue-6609: drives the original app's Windows 11 print dialog");
    return;
  }
  const pdf = tmpPath("issue-6609.pdf");
  const logPath = tmpPath("issue-6609.log");
  rmSync(logPath, { force: true });
  writeFileSync(pdf, makePdf(kPageCount, 600, 0, 0), "latin1");
  const readLog = () => (existsSync(logPath) ? readFileSync(logPath, "utf8") : "");
  const paginations = () => readLog().match(/Win11 print: Paginate pages=\d+ first=\d+/g) ?? [];

  // what the last pagination picked to print, once the dialog stopped asking
  async function settledSelection(nBefore: number): Promise<Selection> {
    await pollUntil(paginations, (p) => p.length > nBefore, { error: "issue-6609: the dialog didn't paginate" });
    let n = -1;
    while (paginations().length !== n) {
      n = paginations().length;
      await sleep(1000);
    }
    const m = /pages=(\d+) first=(\d+)/.exec(paginations()[n - 1])!;
    return { pages: Number(m[1]), first: Number(m[2]) };
  }

  const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, "-page", String(kStartPage), pdf]);
  const pid = proc.pid!;
  const name = basename(pdf);
  let dialogUp = false;
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdPrint"));
    // before Windows 11 there is no such dialog; without a host for it the app
    // finds out late and falls back to the classic one
    const kNoDialog = /Windows 11 dialog=0|ShowPrintUI done status=\d+ shown=0/;
    const log = await pollUntil(readLog, (s) => kNoDialog.test(s) || paginations().length > 0, {
      error: "issue-6609: printing didn't start",
    });
    if (kNoDialog.test(log)) {
      console.log("SKIP issue-6609: no Windows 11 print dialog on this system");
      return;
    }
    dialogUp = true;
    const all = await settledSelection(0);
    if (all.pages !== kPageCount) {
      throw new Error(`issue-6609: prints ${all.pages} pages at first, expected ${kPageCount}`);
    }
    let pager = printDialog(pid, name, "");
    if (pager.page !== kStartPage) {
      throw new Error(`issue-6609: the preview opened on page ${pager.page}, expected ${kStartPage}`);
    }

    // "Current page" is the page the preview shows, also after a custom range
    for (const range of ["", "2-3"]) {
      if (range) {
        printDialog(pid, name, `pick ${kItemCustomRange}`);
        let n = paginations().length;
        printDialog(pid, name, `range ${range}`);
        const custom = await settledSelection(n);
        if (custom.pages !== 2 || custom.first !== 2) {
          throw new Error(`issue-6609: range ${range} prints ${custom.pages} pages from ${custom.first}`);
        }
        pager = printDialog(pid, name, "");
      }
      const shown = pager.page;
      const n = paginations().length;
      printDialog(pid, name, `pick ${kItemCurrentPage}`);
      const current = await settledSelection(n);
      pager = printDialog(pid, name, "");
      const what = `"Current page" on page ${shown}${range ? ` after the range ${range}` : ""}`;
      if (current.pages !== 1 || current.first !== shown) {
        throw new Error(`issue-6609: ${what} prints ${current.pages} page(s) from page ${current.first}`);
      }
      if (pager.page !== shown) {
        throw new Error(`issue-6609: ${what} moved the preview to page ${pager.page}`);
      }
    }
    console.log("issue-6609: OK");
  } finally {
    if (dialogUp) {
      try {
        printDialog(pid, name, "close");
      } catch {
        // the app goes away next and takes the dialog with it
      }
    }
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
