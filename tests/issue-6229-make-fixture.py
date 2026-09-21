# Regenerates tests/issue-6229.jpg: 400x400, left quarter red, rest blue.
# The color edge sits away from the tile edge so the scaler can blend there.
# JPEG on purpose: mupdf caches the decoded image and can hand the whole
# image back for a tile (sub-rect) request.
#   python tests/issue-6229-make-fixture.py
from PIL import Image

W, H = 400, 400
img = Image.new("RGB", (W, H))
px = img.load()
for y in range(H):
    for x in range(W):
        px[x, y] = (255, 0, 0) if x < W // 4 else (0, 0, 255)
img.save("tests/issue-6229.jpg", quality=95, subsampling=0)
