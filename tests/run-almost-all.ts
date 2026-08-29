// Fast regular tests: everything except cases whose time is the test
// (print-to-PDF, LaTeX, debounce/CPU/scroll windows, high-zoom tile settle,
// 400-file HTML TOC, restrict.ini exe copies). Those live in run-all.ts.
//
// Order groups tests that can share a Sumatra session (default -for-testing +
// quadrant -window-pos + -dbg-control) so a later shared-process runner can
// keep one instance. Isolated tests (custom -appdata, session restore, their
// own window placement) come after.
//
// Run:  bun tests/run-almost-all.ts [--no-build] [-silent] [-exe <SumatraPDF.exe>]
//
// Register a new fast test here. Inherently-slow ones go in tests/run-all.ts.
//
// Not here: issue-6000. Its toolbar-tooltip check needs a live pointer (the
// tooltip is TTF_TRACK and the app places it from GetCursorPos), so it fails
// whenever the session is locked or an RDP connection is closed. Run it by
// hand on an unlocked console: bun tests/issue-6000.ts

import { runNamedTests, runSuiteMain, startSuiteProgress, type NamedTest, type SuiteOptions } from "./util.ts";
import { setTestWindowLayout } from "./winapi.ts";
import { beginSharedControlledSession, endSharedControlledSession } from "./win-automation.ts";
import { testit as lintCommandIds } from "./lint-command-ids.ts";
import { testit as lintMingwSources } from "./lint-mingw-sources.ts";
import { testit as buildCli } from "./build-cli.ts";
import { testit as combiningMarkFirst } from "./combining-mark-first.ts";
import { testit as parseTipBrackets } from "./parse-tip-brackets.ts";
import { testit as issue5840 } from "./issue-5840.ts";
import { testit as issue5844 } from "./issue-5844.ts";
import { testit as issue3434 } from "./issue-3434.ts";
import { testit as cmykImageSave } from "./cmyk-image-save.ts";
import { testit as issue1809 } from "./issue-1809.ts";
import { testit as issue4398 } from "./issue-4398.ts";
import { testit as issue6039 } from "./issue-6039.ts";
import { testit as issue5846 } from "./issue-5846.ts";
import { testit as issue5941 } from "./issue-5941.ts";
import { testit as issue2447 } from "./issue-2447.ts";
import { testit as issue476 } from "./issue-476.ts";
import { testit as issue5875 } from "./issue-5875.ts";
import { testit as issue3744 } from "./issue-3744.ts";
import { testit as issue4986 } from "./issue-4986.ts";
import { testit as issue4973 } from "./issue-4973.ts";
import { testit as issue2083 } from "./issue-2083.ts";
import { testit as issue5329 } from "./issue-5329.ts";
import { testit as issue5718 } from "./issue-5718.ts";
import { testit as issue5734 } from "./issue-5734.ts";
import { testit as issue5736 } from "./issue-5736.ts";
import { testit as issue5529 } from "./issue-5529.ts";
import { testit as issue2629 } from "./issue-2629.ts";
import { testit as issue4684 } from "./issue-4684.ts";
import { testit as issue5922 } from "./issue-5922.ts";
import { testit as issue5924 } from "./issue-5924.ts";
import { testit as issue5926 } from "./issue-5926.ts";
import { testit as issue1914 } from "./issue-1914.ts";
import { testit as issue1198 } from "./issue-1198.ts";
import { testit as issue2568 } from "./issue-2568.ts";
import { testit as issue2799 } from "./issue-2799.ts";
import { testit as findMatchSelect } from "./issue-find-match-select.ts";
import { testit as findResultsSorted } from "./find-results-sorted.ts";
import { testit as findWindowLayout } from "./find-window-layout.ts";
import { testit as findUiState } from "./find-ui-state.ts";
import { testit as issue5874 } from "./issue-5874.ts";
import { testit as issue6055 } from "./issue-6055.ts";
import { testit as sessionRestoreSearch } from "./session-restore-search.ts";
import { testit as issue2252 } from "./issue-2252.ts";
import { testit as issue5869 } from "./issue-5869.ts";
import { testit as issue5881 } from "./issue-5881.ts";
import { testit as rectSelectionDrag } from "./rect-selection-drag.ts";
import { testit as issue5938 } from "./issue-5938.ts";
import { testit as issue1085 } from "./issue-1085.ts";
import { testit as issue2873 } from "./issue-2873.ts";
import { testit as issue5937 } from "./issue-5937.ts";
import { testit as issue933 } from "./issue-933.ts";
import { testit as issue3219 } from "./issue-3219.ts";
import { testit as issue5404 } from "./issue-5404.ts";
import { testit as issue5537 } from "./issue-5537.ts";
import { testit as issue5597 } from "./issue-5597.ts";
import { testit as issue5642 } from "./issue-5642.ts";
import { testit as issue5665 } from "./issue-5665.ts";
import { testit as issue5677 } from "./issue-5677.ts";
import { testit as issue5681 } from "./issue-5681.ts";
import { testit as issue1678 } from "./issue-1678.ts";
import { testit as issue1724 } from "./issue-1724.ts";
import { testit as issue2239 } from "./issue-2239.ts";
import { testit as issue1744 } from "./issue-1744.ts";
import { testit as issue3415 } from "./issue-3415.ts";
import { testit as issue5944 } from "./issue-5944.ts";
import { testit as issue1201 } from "./issue-1201.ts";
import { testit as issue5724 } from "./issue-5724.ts";
import { testit as issue4705 } from "./issue-4705.ts";
import { testit as issue1189 } from "./issue-1189.ts";
import { testit as issue5871 } from "./issue-5871.ts";
import { testit as issue5873 } from "./issue-5873.ts";
import { testit as ghsaJf4vRw66J4w2 } from "./security-ghsa-jf4v-rw66-j4w2.ts";
import { testit as issue5317 } from "./issue-5317.ts";
import { testit as issue5694 } from "./issue-5694.ts";
import { testit as issue3731 } from "./issue-3731.ts";
import { testit as issue5095 } from "./issue-5095.ts";
import { testit as issue5751 } from "./issue-5751.ts";
import { testit as issue5780 } from "./issue-5780.ts";
import { testit as issue2254 } from "./issue-2254.ts";
import { testit as issue5950 } from "./issue-5950.ts";
import { testit as issue5993 } from "./issue-5993.ts";
import { testit as issue5845 } from "./issue-5845.ts";
import { testit as issue5870 } from "./issue-5870.ts";
import { testit as ghsaP2ph2rvmQ37m } from "./security-ghsa-p2ph-2rvm-q37m.ts";
import { testit as issue4753 } from "./issue-4753.ts";
import { testit as issue4055 } from "./issue-4055.ts";
import { testit as issue2165 } from "./issue-2165.ts";
import { testit as issue3472 } from "./issue-3472.ts";
import { testit as pdfOnlyMenuItems } from "./pdf-only-menu-items.ts";
import { testit as issue2258 } from "./issue-2258.ts";
import { testit as issue2737 } from "./issue-2737.ts";
import { testit as issue6030 } from "./issue-6030.ts";
import { testit as issue6050 } from "./issue-6050.ts";
import { testit as issue6088 } from "./issue-6088.ts";
import { testit as annotContentsClickAway } from "./annot-contents-click-away.ts";
import { testit as issue6093 } from "./issue-6093.ts";
import { testit as toolbarHoverDropdown } from "./toolbar-hover-dropdown.ts";
import { testit as issue6095 } from "./issue-6095.ts";
import { testit as issue1106 } from "./issue-1106.ts";
import { testit as issue814 } from "./issue-814.ts";
import { testit as issue1422 } from "./issue-1422.ts";
import { testit as issue1438 } from "./issue-1438.ts";
import { testit as issue1136 } from "./issue-1136.ts";
import { testit as issue893 } from "./issue-893.ts";
import { testit as issue1699 } from "./issue-1699.ts";
import { testit as issue1998 } from "./issue-1998.ts";
import { testit as issue2199 } from "./issue-2199.ts";
import { testit as issue906 } from "./issue-906.ts";
import { testit as issue3560 } from "./issue-3560.ts";
import { testit as issue3591 } from "./issue-3591.ts";
import { testit as issue4576 } from "./issue-4576.ts";
import { testit as issue5850 } from "./issue-5850.ts";

import { testit as issue6028 } from "./issue-6028.ts";
import { testit as issue6062 } from "./issue-6062.ts";
import { testit as issue5969 } from "./issue-5969.ts";
import { testit as issue5867 } from "./issue-5867.ts";
import { testit as issue5868 } from "./issue-5868.ts";
import { testit as issue5899 } from "./issue-5899.ts";
import { testit as issue5907 } from "./issue-5907.ts";
import { testit as settingsPersist } from "./settings-persist.ts";
import { testit as issue5970 } from "./issue-5970.ts";
import { testit as issue5971 } from "./issue-5971.ts";
import { testit as issue5933 } from "./issue-5933.ts";
import { testit as issue6037 } from "./issue-6037.ts";
import { testit as issue6043 } from "./issue-6043.ts";
import { testit as issue6045 } from "./issue-6045.ts";
import { testit as issue6046 } from "./issue-6046.ts";
import { testit as issue6048 } from "./issue-6048.ts";
import { testit as issue6053 } from "./issue-6053.ts";
import { testit as issue6054 } from "./issue-6054.ts";
import { testit as favoritesTabAfterDocumentClose } from "./favorites-tab-after-document-close.ts";
import { testit as movePolygonPolylineInk } from "./move-polygon-polyline-ink.ts";

import { testit as pdfEditToolbarInteraction } from "./pdf-edit-toolbar-interaction.ts";
import { testit as annotCopyPaste } from "./annot-copy-paste.ts";
import { testit as annotCutPaste } from "./annot-cut-paste.ts";
import { testit as annotUndoRedo } from "./annot-undo-redo.ts";
import { testit as textAnnotationPlacement } from "./text-annotation-placement.ts";
import { testit as freeTextAnnotationPlacement } from "./free-text-annotation-placement.ts";
import { testit as freeTextEditToolbar } from "./free-text-edit-toolbar.ts";
import { testit as createAnnotEntersEditMode } from "./create-annot-enters-edit-mode.ts";
import { testit as exitEditPdfDeselects } from "./exit-edit-pdf-deselects.ts";
import { testit as freeTextInPlaceEdit } from "./free-text-in-place-edit.ts";
import { testit as freeTextEditMatchesRender } from "./free-text-edit-matches-render.ts";
import { testit as annotMoveableTypes } from "./annot-moveable-types.ts";
import { testit as stampCaretAnnotationPlacement } from "./stamp-caret-annotation-placement.ts";
import { testit as lineAnnotationPlacement } from "./line-annotation-placement.ts";
import { testit as polylineAnnotationPlacement } from "./polyline-annotation-placement.ts";
import { testit as shapeAnnotationPlacement } from "./shape-annotation-placement.ts";
import { testit as redactAnnotations } from "./redact-annotations.ts";
import { testit as inkAnnotationPlacement } from "./ink-annotation-placement.ts";
import { testit as issue5956 } from "./issue-5956.ts";
import { testit as imageOnlyPaletteItems } from "./image-only-palette-items.ts";
import { testit as commandPaletteShortcutFilter } from "./command-palette-shortcut-filter.ts";

import { testit as issue5968 } from "./issue-5968.ts";
import { testit as issue5978 } from "./issue-5978.ts";
import { testit as issue5963 } from "./issue-5963.ts";
import { testit as issue5964 } from "./issue-5964.ts";
import { testit as issue5965 } from "./issue-5965.ts";
import { testit as issue1315 } from "./issue-1315.ts";
import { testit as issue5581 } from "./issue-5581.ts";
import { testit as issue5991 } from "./issue-5991.ts";
import { testit as issue5980 } from "./issue-5980.ts";
import { testit as issue5982 } from "./issue-5982.ts";
import { testit as issue6035 } from "./issue-6035.ts";
import { testit as issue5984 } from "./issue-5984.ts";
import { testit as issue5979 } from "./issue-5979.ts";
import { testit as findBarDpi } from "./find-bar-dpi.ts";
import { testit as issue6032 } from "./issue-6032.ts";
import { testit as issue5988 } from "./issue-5988.ts";
import { testit as issue5989 } from "./issue-5989.ts";
import { testit as issue5995 } from "./issue-5995.ts";
import { testit as issue5997 } from "./issue-5997.ts";

import { testit as issue6001 } from "./issue-6001.ts";
import { testit as issue4157 } from "./issue-4157.ts";
import { testit as issue5512 } from "./issue-5512.ts";
import { testit as issue6005 } from "./issue-6005.ts";
import { testit as issue4655 } from "./issue-4655.ts";
import { testit as issue4315 } from "./issue-4315.ts";
import { testit as issue4662 } from "./issue-4662.ts";
import { testit as issue6013 } from "./issue-6013.ts";
import { testit as issue6018 } from "./issue-6018.ts";
import { testit as selectionToolbarStays } from "./selection-toolbar-stays.ts";
import { testit as selectionToolbarMove } from "./selection-toolbar-move.ts";
import { testit as selectionFontBbox } from "./selection-font-bbox.ts";
import { testit as issue6023 } from "./issue-6023.ts";
import { testit as issue6017 } from "./issue-6017.ts";
import { testit as issue6025 } from "./issue-6025.ts";
import { testit as issue5946 } from "./issue-5946.ts";
import { testit as issue6012 } from "./issue-6012.ts";
import { testit as advSettingsFreshDefaults } from "./adv-settings-fresh-defaults.ts";

async function annotationClipboardTests(): Promise<void> {
  beginSharedControlledSession();
  try {
    await annotUndoRedo();
    await annotCopyPaste();
    await annotCutPaste();
  } finally {
    await endSharedControlledSession();
  }
}

export const tests: NamedTest[] = [
  // first: it drives the home page, whose thumbnail selection follows the
  // mouse, so it is the one test that cares what the machine was doing before
  ["issue-5978", issue5978],
  ["image-only-palette-items", imageOnlyPaletteItems],
  ["issue-5868", issue5868],
  ["issue-893", issue893],
  ["issue-2252", issue2252],
  ["issue-5944", issue5944],
  ["issue-5993", issue5993],
  ["issue-6039", issue6039],
  ["issue-5933", issue5933],
  ["issue-4398", issue4398],
  ["issue-5964", issue5964],
  ["issue-6013", issue6013],
  ["issue-5956", issue5956],
  ["issue-5989", issue5989],

  // --- no Sumatra process -------------------------------------------------
  ["lint-command-ids", lintCommandIds],
  ["lint-mingw-sources", lintMingwSources],
  ["build-cli", buildCli],
  ["parse-tip-brackets", parseTipBrackets],
  ["combining-mark-first", combiningMarkFirst],
  ["issue-5840", issue5840],
  ["issue-5844", issue5844],
  ["issue-3434", issue3434],
  ["cmyk-image-save", cmykImageSave],
  ["issue-1809", issue1809],
  ["issue-5846", issue5846],
  ["issue-5941", issue5941],
  ["issue-2447", issue2447],
  ["issue-476", issue476],
  ["issue-5875", issue5875],

  // --- default session: -for-testing + quadrant window + -dbg-control ----
  ["issue-6037", issue6037],
  ["issue-6043", issue6043],
  ["issue-6046", issue6046],
  ["issue-6048", issue6048],
  ["issue-6053", issue6053],
  ["issue-6054", issue6054],
  ["move-polygon-polyline-ink", movePolygonPolylineInk],
  ["pdf-edit-toolbar-interaction", pdfEditToolbarInteraction],
  ["annotation clipboard tests", annotationClipboardTests],
  ["text-annotation-placement", textAnnotationPlacement],
  ["free-text-annotation-placement", freeTextAnnotationPlacement],
  ["free-text-edit-toolbar", freeTextEditToolbar],
  ["create-annot-enters-edit-mode", createAnnotEntersEditMode],
  ["exit-edit-pdf-deselects", exitEditPdfDeselects],
  ["free-text-in-place-edit", freeTextInPlaceEdit],
  ["free-text-edit-matches-render", freeTextEditMatchesRender],
  ["annot-moveable-types", annotMoveableTypes],
  ["stamp-caret-annotation-placement", stampCaretAnnotationPlacement],
  ["line-annotation-placement", lineAnnotationPlacement],
  ["polyline-annotation-placement", polylineAnnotationPlacement],
  ["shape-annotation-placement", shapeAnnotationPlacement],
  ["redact-annotations", redactAnnotations],
  ["ink-annotation-placement", inkAnnotationPlacement],
  ["issue-1315", issue1315],
  ["issue-5581", issue5581],
  ["issue-5991", issue5991],
  ["issue-4973", issue4973],
  ["issue-2083", issue2083],
  ["issue-5329", issue5329],
  ["issue-5718", issue5718],
  ["issue-5734", issue5734],
  ["issue-5736", issue5736],
  ["issue-5529", issue5529],
  ["issue-2629", issue2629],
  ["issue-4684", issue4684],
  ["issue-5922", issue5922],
  ["issue-5924", issue5924],
  ["issue-5926", issue5926],
  ["issue-1914", issue1914],
  ["issue-1198", issue1198],
  ["issue-2568", issue2568],
  ["issue-2799", issue2799],
  ["issue-find-match-select", findMatchSelect],
  ["find-results-sorted", findResultsSorted],
  ["find-window-layout", findWindowLayout],
  ["find-ui-state", findUiState],
  ["issue-5874", issue5874],
  ["issue-6055", issue6055],
  ["session-restore-search", sessionRestoreSearch],
  ["issue-5869", issue5869],
  ["issue-5881", issue5881],
  ["rect-selection-drag", rectSelectionDrag],
  ["issue-5938", issue5938],
  ["issue-1085", issue1085],
  ["issue-2873", issue2873],
  ["issue-5937", issue5937],
  ["issue-933", issue933],
  ["issue-3219", issue3219],
  ["issue-5404", issue5404],
  ["issue-5537", issue5537],
  ["issue-5597", issue5597],
  ["issue-5642", issue5642],
  ["issue-5665", issue5665],
  ["issue-5677", issue5677],
  ["issue-5681", issue5681],
  ["issue-1678", issue1678],
  ["issue-1724", issue1724],
  ["issue-2239", issue2239],
  ["issue-1744", issue1744],
  ["issue-3415", issue3415],
  ["issue-1201", issue1201],
  ["issue-5724", issue5724],
  ["issue-4705", issue4705],
  ["issue-1189", issue1189],
  ["issue-5871", issue5871],
  ["issue-5873", issue5873],
  ["security-ghsa-jf4v-rw66-j4w2", ghsaJf4vRw66J4w2],
  ["issue-5317", issue5317],
  ["issue-5694", issue5694],
  ["issue-1699", issue1699],
  ["issue-2254", issue2254],
  ["issue-5950", issue5950],
  ["issue-3472", issue3472],
  ["pdf-only-menu-items", pdfOnlyMenuItems],
  ["security-ghsa-p2ph-2rvm-q37m", ghsaP2ph2rvmQ37m],
  ["issue-5780", issue5780],
  ["issue-5845", issue5845],
  ["issue-5870", issue5870],
  ["issue-5963", issue5963],
  ["issue-5965", issue5965],

  // --- isolated session: -appdata, saveSettings, or own window placement -
  ["favorites-tab-after-document-close", favoritesTabAfterDocumentClose],
  ["issue-6045", issue6045],
  ["issue-3744", issue3744],
  ["issue-4986", issue4986],
  ["issue-5095", issue5095],
  ["issue-3731", issue3731],
  ["issue-5751", issue5751],
  ["issue-4753", issue4753],
  ["issue-4055", issue4055],
  ["issue-2165", issue2165],
  ["issue-2258", issue2258],
  ["issue-2737", issue2737],
  ["issue-6030", issue6030],
  ["issue-6050", issue6050],
  ["issue-6088", issue6088],
  ["annot-contents-click-away", annotContentsClickAway],
  ["issue-6093", issue6093],
  ["toolbar-hover-dropdown", toolbarHoverDropdown],
  ["issue-6095", issue6095],
  ["issue-1106", issue1106],
  ["issue-814", issue814],
  ["issue-1422", issue1422],
  ["issue-1438", issue1438],
  ["issue-1136", issue1136],
  ["issue-1998", issue1998],
  ["issue-2199", issue2199],
  ["issue-906", issue906],
  ["issue-3560", issue3560],
  ["issue-3591", issue3591],
  ["issue-4576", issue4576],
  ["issue-5850", issue5850],
  ["issue-6028", issue6028],
  ["issue-6062", issue6062],
  ["issue-5969", issue5969],
  ["issue-5867", issue5867],
  ["issue-5899", issue5899],
  ["issue-5907", issue5907],
  ["settings-persist", settingsPersist],
  ["issue-5970", issue5970],
  ["issue-5971", issue5971],
  ["command-palette-shortcut-filter", commandPaletteShortcutFilter],
  ["issue-5968", issue5968],
  ["issue-5980", issue5980],
  ["issue-5982", issue5982],
  ["issue-6035", issue6035],
  ["issue-5984", issue5984],
  ["issue-5979", issue5979],
  ["find-bar-dpi", findBarDpi],
  ["issue-6032", issue6032],
  ["issue-5988", issue5988],
  ["issue-5995", issue5995],
  ["issue-5997", issue5997],
  ["issue-6001", issue6001],
  ["issue-4157", issue4157],
  ["issue-5512", issue5512],
  ["issue-6005", issue6005],
  ["issue-4655", issue4655],
  ["issue-4315", issue4315],
  ["issue-4662", issue4662],
  ["issue-6018", issue6018],
  ["selection-toolbar-stays", selectionToolbarStays],
  ["selection-toolbar-move", selectionToolbarMove],
  ["selection-font-bbox", selectionFontBbox],
  ["issue-6023", issue6023],
  ["issue-6017", issue6017],
  ["issue-6025", issue6025],
  ["issue-5946", issue5946],
  ["issue-6012", issue6012],
  ["adv-settings-fresh-defaults", advSettingsFreshDefaults],
];

export async function testit(opts?: SuiteOptions): Promise<void> {
  startSuiteProgress(tests.length);
  // the right half of the screen: this suite is run by a person, so the window
  // stays out of the way (run-github-ci.ts asks for the whole work area)
  setTestWindowLayout("rightHalf");
  await runNamedTests(tests, { heading: "run-almost-all", ...opts });
}

if (import.meta.main) {
  await runSuiteMain(testit);
}
