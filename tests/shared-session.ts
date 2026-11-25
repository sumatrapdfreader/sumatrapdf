// Tests that open one document and pass only -view / -zoom / -page. They run
// as their own entries, after everything else, in one Sumatra process.

import type { NamedTest } from "./util.ts";
import { setProcessReuse } from "./process-reuse.ts";

const shareNames = new Set(
  `
issue-6359
touch-long-press-select
pen-input
ctx-menu-pick
issue-6113
issue-6405
pdf-only-menu-items
issue-6477
issue-6373
issue-6360
issue-6431
issue-5924
annot-text-popup-reload
issue-6232-path-edit
issue-6070
issue-6372
issue-6433
issue-6435
convert-epub-to-pdf
sidebar-thumbnails-wheel
issue-6312
palette-prefix-keys
issue-6279
issue-2423
tab-ctx-menu-close
issue-6229
issue-6245
issue-6246
issue-6248
ad-hoc-md-missing-file
issue-6397
issue-5993
issue-4276
embedded-image-attachment
issue-4705
issue-6101
issue-893
cmyk-image-save
issue-5944
issue-5844
issue-3434
issue-1809
issue-6037
issue-6046
issue-4973
issue-6241
issue-4684
issue-6302
issue-5922
issue-1914
issue-1198
issue-6190
find-window-layout
issue-5407
issue-5537
issue-5597
issue-5642
issue-1724
issue-2239
issue-1744
issue-3415
issue-1189
issue-1699
issue-3585
issue-1846
xmp-properties
issue-6364
issue-6380
markdown-path-traversal
issue-5780
toc-expand-chapter
issue-6265
issue-6266
issue-6330
issue-6332
issue-6339
issue-1293
epub-first-page-image
css-invalid-color
epub-pre-wraps
issue-6280
issue-6322
issue-6324
issue-6136
issue-1930
issue-1422
issue-2199
issue-6470
issue-6472
issue-6232-filter
issue-6232-refresh
issue-6238
issue-1841
render-selections-8bpp
wheel-while-closing
issue-6259
issue-6261
debug-show-notif
menu-rebuild-burst
command-palette-rows-built
tab-tooltip-size
copy-rect-selection-image
issue-5512
issue-6005
issue-4315
issue-6399
issue-6400
issue-6424
issue-6421
keyboard-select-yank-empty
selection-font-bbox
issue-6023
issue-6017
adv-settings-enter
hex-view
issue-6132
toolbar-tab-switch-pos
issue-trim-margins
merge-close-paint
file-link-no-dest
issue-6441
issue-6480
translate-dialog-owner
notif-groups
issue-6486
issue-6488
issue-6489
issue-6490
issue-6492
issue-6496
copy-join-lines
tab-switch-ends-modes
background-tab-reload
caption-capture-loss
menu-bar-armed-deactivate
issue-6539
issue-6541
issue-6540
issue-6570
issue-6542
issue-6544
issue-6549
issue-6550
issue-6556
issue-6563
issue-6567
issue-6568
f7-ends-placement
trackpad-diagonal-scroll
trackpad-pinch-zoom
toolbar-right-click
save-annots-same-path
command-palette-toc-reload
ctx-menu-scroll
`
    .trim()
    .split(/\s+/),
);

export function shouldShareProcess(name: string): boolean {
  return shareNames.has(name);
}

export function orderForReuse(tests: NamedTest[]): NamedTest[] {
  const head: NamedTest[] = [];
  const tail: NamedTest[] = [];
  for (const t of tests) {
    if (shareNames.has(t[0])) {
      tail.push(t);
    } else {
      head.push(t);
    }
  }
  return [...head, ...tail];
}

let armed = false;

export async function noteReuseTest(name: string): Promise<void> {
  const want = shareNames.has(name);
  if (want === armed) {
    return;
  }
  if (armed) {
    const { endSharedControlledSession } = await import("./win-automation.ts");
    await endSharedControlledSession();
  }
  setProcessReuse(want);
  armed = want;
}

export async function finishProcessReuse(): Promise<void> {
  if (!armed) {
    return;
  }
  armed = false;
  setProcessReuse(false);
  const { endSharedControlledSession } = await import("./win-automation.ts");
  await endSharedControlledSession();
}
