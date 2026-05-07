#!/usr/bin/env python3
"""
deploy_prettysumatra_icons.py

Copiar/convertir los PNG/ICO generados en gfx/PrettySumatra hacia gfx/ (raíz del proyecto),
haciendo backups automáticos de los ficheros que se sobrescriban.

Uso:
  python deploy_prettysumatra_icons.py --source ../gfx/PrettySumatra --target ../gfx --backup

"""
from pathlib import Path
import time
import shutil
from PIL import Image
import sys


def backup(p: Path):
    ts = int(time.time())
    b = p.with_name(p.name + f".bak.{ts}")
    p.rename(b)
    print(f"Backup: {p} -> {b}")


def find_pngs(src: Path, prefix: str):
    # find pngs whose stem startswith prefix (case-insensitive) or startswith prefix + ' '
    pref = prefix.lower()
    out = []
    for p in src.glob('*.png'):
        stem = p.stem.lower()
        if stem.startswith(pref) or stem.startswith(pref + ' ') or stem.replace(' 1','').startswith(pref):
            out.append(p)
    return sorted(out, key=lambda p: p.stem)


def sumatrapdf_size_pngs(src: Path):
    sizes = ['256', '128', '64', '48', '32', '16']
    out = []
    for size in sizes:
        p = src / f'SumatraPDF-{size}.png'
        if p.exists():
            out.append(p)
    return out


def create_ico_from_pngs(png_paths, out_path):
    imgs = []
    for p in png_paths:
        try:
            im = Image.open(p).convert('RGBA')
            imgs.append(im)
        except Exception as e:
            print(f"Error abriendo {p}: {e}")
    if not imgs:
        print(f"No hay PNGs para {out_path.name}")
        return False
    largest = max(imgs, key=lambda im: im.size[0])
    sizes = [(im.size[0], im.size[1]) for im in imgs]
    largest.save(out_path, format='ICO', sizes=sizes)
    print(f"Wrote ICO {out_path} sizes={sizes}")
    return True


def main(argv=None):
    src = Path('../gfx/PrettySumatra').resolve()
    tgt = Path('../gfx').resolve()
    if not src.exists():
        print(f"Source folder not found: {src}")
        sys.exit(1)
    if not tgt.exists():
        print(f"Target folder not found: {tgt}")
        sys.exit(1)

    # Map program PNGs: SumatraPDF-*.png -> SumatraPDF-<size>x<size>x32.png
    mapping = {
        '256': 'SumatraPDF-256x256x32.png',
        '128': 'SumatraPDF-128x128x32.png',
        '64':  'SumatraPDF-64x64x32.png',
        '48':  'SumatraPDF-48x48x32.png',
        '32':  'SumatraPDF-32x32x32.png',
        '16':  'SumatraPDF-16x16x32.png',
    }

    gfxalt = tgt / 'gfxalt'
    if not gfxalt.exists():
        print(f"Target gfxalt folder not found: {gfxalt}")
        sys.exit(1)

    # Replace program PNGs if generated versions exist
    for size, tgtname in mapping.items():
        src_png = src / f"SumatraPDF-{size}.png"
        if src_png.exists():
            tgt_png = tgt / tgtname
            if tgt_png.exists():
                backup(tgt_png)
            shutil.copy2(src_png, tgt_png)
            print(f"Copied {src_png.name} -> {tgt_png}")

            gfxalt_png = gfxalt / tgtname
            if gfxalt_png.exists():
                backup(gfxalt_png)
            shutil.copy2(src_png, gfxalt_png)
            print(f"Copied {src_png.name} -> {gfxalt_png}")

    # Replace program ICO and the smaller app icon used by rc
    src_ico = src / 'SumatraPDF.ico'
    if src_ico.exists():
        tgt_ico = tgt / 'SumatraPDF.ico'
        if tgt_ico.exists():
            backup(tgt_ico)
        shutil.copy2(src_ico, tgt_ico)
        print(f"Copied ICO {src_ico.name} -> {tgt_ico}")

        # build a smaller ico from the same PNG set for the app icon
        smaller_ico = tgt / 'SumatraPDF-smaller.ico'
        if smaller_ico.exists():
            backup(smaller_ico)
        create_ico_from_pngs(sumatrapdf_size_pngs(src), smaller_ico)

        gfxalt_smaller = gfxalt / 'SumatraPDF-smaller.ico'
        if gfxalt_smaller.exists():
            backup(gfxalt_smaller)
        create_ico_from_pngs(sumatrapdf_size_pngs(src), gfxalt_smaller)

    # Now create/replace filetype ICOs based on PNGs
    ico_targets = [p for p in tgt.glob('*.ico') if p.name not in ('SumatraPDF.ico','SumatraPDF-smaller.ico')]
    for ico in ico_targets:
        base = ico.stem
        pngs = find_pngs(src, base)
        if not pngs:
            print(f"No PNGs found for {base}, skipping {ico.name}")
            continue
        # backup existing
        backup(ico)
        create_ico_from_pngs(pngs, ico)

    # Also deploy filetype ICOs to gfxalt, because SumatraPDF.rc references gfxalt
    gfxalt_ico_targets = [p for p in gfxalt.glob('*.ico') if p.name not in ('SumatraPDF.ico','SumatraPDF-smaller.ico')]
    for ico in gfxalt_ico_targets:
        base = ico.stem
        pngs = find_pngs(src, base)
        if not pngs:
            print(f"No PNGs found for {base}, skipping {ico.name}")
            continue
        backup(ico)
        create_ico_from_pngs(pngs, ico)

    print('Deploy completo.')


if __name__ == '__main__':
    main()
