# Regenerates tests/rtl-leading-space.pdf: "פח עובי 5" written the way
# SOLIDWORKS writes it. The space comes first in the content stream but is
# drawn between the Hebrew word and the number.
#   python tests/rtl-leading-space-make-fixture.py
from pathlib import Path

import pymupdf

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
OUT = Path(__file__).with_name("rtl-leading-space.pdf")
SIZE = 16
WORD = "פח עובי"
RIGHT_X = 260.0
BASELINE_Y = 45

doc = pymupdf.open()
page = doc.new_page(width=300, height=80)
page.insert_font(fontname="dv", fontfile=FONT)
font = pymupdf.Font(fontfile=FONT)


def width(ch):
    return font.text_length(ch, fontsize=SIZE)


# visual layout, right to left: word, space, "5"
x = RIGHT_X
word = []
for ch in WORD:
    x -= width(ch)
    word.append((ch, x))
space_x = x - width(" ")
five_x = space_x - width("5")

# content stream order: space, word, number
for ch, cx in [(" ", space_x)] + word + [("5", five_x)]:
    page.insert_text((cx, BASELINE_Y), ch, fontname="dv", fontsize=SIZE)

doc.subset_fonts()
doc.save(OUT, garbage=4, deflate=True)
