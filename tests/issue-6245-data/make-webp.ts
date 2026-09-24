// Regenerates the WebP fixtures of tests/issue-6245.ts. Needs sharp, which is
// not a repo dependency: run it from a scratch dir after `bun add sharp`:
//   bun <repo>/tests/issue-6245-data/make-webp.ts <repo>/tests/issue-6245-data
import sharp from "sharp";

// left half blue, right half orange (translucent in the alpha variant)
const LEFT = [40, 90, 200];
const RIGHT = [230, 120, 30];
const RIGHT_ALPHA = 128;

function halves(w: number, h: number, channels: number): Buffer {
  const b = Buffer.alloc(w * h * channels);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const px = x < w / 2 ? [...LEFT, 255] : [...RIGHT, RIGHT_ALPHA];
      b.set(px.slice(0, channels), (y * w + x) * channels);
    }
  }
  return b;
}

// libvips writes the EXIF chunk as "Exif\0\0" + TIFF; the WebP spec (and our
// parser) wants the TIFF header right at the start of the chunk
function stripExifPrefix(webp: Buffer): Buffer {
  const prefix = Buffer.from("Exif\0\0", "latin1");
  const at = webp.indexOf("EXIF", 12, "latin1");
  if (at < 0 || !webp.subarray(at + 8, at + 8 + prefix.length).equals(prefix)) {
    throw new Error("no Exif-prefixed EXIF chunk");
  }
  const out = Buffer.concat([webp.subarray(0, at + 8), webp.subarray(at + 8 + prefix.length)]);
  out.writeUInt32LE(webp.readUInt32LE(at + 4) - prefix.length, at + 4);
  out.writeUInt32LE(webp.readUInt32LE(4) - prefix.length, 4);
  return out;
}

const dir = process.argv[2]!;
const W = 64;
const H = 64;

await sharp(halves(W, H, 3), { raw: { width: W, height: H, channels: 3 } })
  .webp({ quality: 90 })
  .toFile(`${dir}/halves.webp`);

await sharp(halves(W, H, 4), { raw: { width: W, height: H, channels: 4 } })
  .webp({ lossless: true })
  .toFile(`${dir}/halves-alpha.webp`);

// stored 64x32 with EXIF orientation 6 (rotate 90° clockwise): shows as 32x64
const rot = await sharp(halves(64, 32, 3), { raw: { width: 64, height: 32, channels: 3 } })
  .webp({ quality: 90 })
  .withMetadata({ orientation: 6 })
  .toBuffer();
await Bun.write(`${dir}/halves-rot90.webp`, stripExifPrefix(rot));
