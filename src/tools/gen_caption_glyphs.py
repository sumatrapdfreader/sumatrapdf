#!/usr/bin/env python3
"""Generate CaptionGlyphs.cpp path data from Segoe Fluent Icons (Win11 style).

Run on a machine with C:\\Windows\\Fonts\\SegoeIcons.ttf, then check in the
updated src/CaptionGlyphs.cpp.
"""

from __future__ import annotations

import sys
from pathlib import Path

from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont

FONT_PATH = Path(r"C:\Windows\Fonts\SegoeIcons.ttf")
CODEPOINTS = {
    0xE921: "BuildMinimizePath",
    0xE922: "BuildMaximizePath",
    0xE923: "BuildRestorePath",
    0xE8BB: "BuildClosePath",
}


class CubicPen(BasePen):
    def __init__(self, glyphSet):
        super().__init__(glyphSet)
        self.ops: list[tuple[str, tuple]] = []

    def _moveTo(self, pt):
        self.ops.append(("moveTo", pt))

    def _lineTo(self, pt):
        self.ops.append(("lineTo", pt))

    def _curveToOne(self, bcp1, bcp2, pt):
        self.ops.append(("curveTo", (bcp1, bcp2, pt)))

    def _closePath(self):
        self.ops.append(("closePath", ()))


def fmt(v: float) -> str:
    if abs(v - round(v)) < 1e-6:
        return str(int(round(v)))
    return f"{v:.6g}f"


def emit_path_fn(name: str, ops: list[tuple[str, tuple]]) -> str:
    lines = [f"static void {name}(Gdiplus::GraphicsPath* path) {{", "    PathBuilder b{path};"]
    for op, args in ops:
        if op == "moveTo":
            x, y = args
            lines.append(f"    b.MoveTo({fmt(x)}, {fmt(y)});")
        elif op == "lineTo":
            x, y = args
            lines.append(f"    b.LineTo({fmt(x)}, {fmt(y)});")
        elif op == "curveTo":
            (x1, y1), (x2, y2), (x3, y3) = args
            lines.append(
                f"    b.CurveTo({fmt(x1)}, {fmt(y1)}, {fmt(x2)}, {fmt(y2)}, {fmt(x3)}, {fmt(y3)});"
            )
        elif op == "closePath":
            lines.append("    b.ClosePath();")
        else:
            raise ValueError(f"unexpected op {op}")
    lines.append("}")
    return "\n".join(lines)


def main() -> int:
    if not FONT_PATH.is_file():
        print(f"font not found: {FONT_PATH}", file=sys.stderr)
        return 1

    font = TTFont(str(FONT_PATH))
    glyph_set = font.getGlyphSet()
    cmap = font.getBestCmap()

    parts = []
    for cp, fn in CODEPOINTS.items():
        if cp not in cmap:
            print(f"codepoint U+{cp:04X} missing from font", file=sys.stderr)
            return 1
        pen = CubicPen(glyph_set)
        glyph_set[cmap[cp]].draw(pen)
        parts.append(emit_path_fn(fn, pen.ops))

    cpp_path = Path(__file__).resolve().parents[1] / "src" / "CaptionGlyphs.cpp"
    text = cpp_path.read_text(encoding="utf-8")

    start = text.index("static void BuildMinimizePath")
    end = text.index("static void BuildCaptionSysButtonPath")
    generated = "\n\n".join(parts) + "\n\n"
    new_text = text[:start] + generated + text[end:]
    cpp_path.write_text(new_text, encoding="utf-8", newline="\n")
    print(f"updated {cpp_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
