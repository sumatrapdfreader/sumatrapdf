# Regenerates tests/issue-6225.jpg: 400x2000, red/blue bands 25px high.
# JPEG on purpose: mupdf decodes a JPEG sub-rect, so a render request that
# asks for the estimated (shorter) box gets only the top of the page.
#   python tests/issue-6225-make-fixture.py
from PIL import Image

W, H, BAND = 400, 2000, 25
img = Image.new("RGB", (W, H))
px = img.load()
for y in range(H):
    c = (255, 0, 0) if (y // BAND) % 2 == 0 else (0, 0, 255)
    for x in range(W):
        px[x, y] = c
img.save("tests/issue-6225.jpg", quality=95, subsampling=0)
