Real-world XFA PDF fixtures for tests/ad-hoc-xfa-corpus.ts and
tests/ad-hoc-xfa-f1040-visual.ts.

issue-1294-xfa.pdf
  Source: SumatraPDF regression suite "1294 - XFA form.pdf" (GitHub issue #1294).
  Pure-XFA sample; current baseline: has_xfa=1, pure_xfa=1, valid=1.

ad-hoc-f1040.pdf
  Source: sumtest/annots/f1040.pdf (IRS Form 1040 with hybrid AcroForm + XFA).
  Current baseline: has_xfa=1, pure_xfa=0, valid=1, 69 fields page 0, 47 page 1.

ad-hoc-f1040-p0.png / ad-hoc-f1040-p1.png
  Reference renders at 100% zoom (human inspection). Visual regression uses the
  paired *.metrics.json files (dimensions + sampled pixel coverage), not
  byte-identical PNG comparison.

Optional external corpus entries (not stored here) are listed in ad-hoc-xfa-corpus.ts.