#!/usr/bin/env python3
"""
convert_prettysumatra_icons.py

Convierte SVGs en `gfx/PrettySumatra` a PNG en varias resoluciones
y genera un archivo .ico a partir del SVG principal.

Requisitos:
  pip install cairosvg pillow

Uso ejemplo:
  python convert_prettysumatra_icons.py --source ../../gfx/PrettySumatra --ico SumatraPDF.svg --overwrite

El script por defecto realiza backups de archivos existentes renombrándolos con sufijo
`.bak.<timestamp>` cuando se usa `--overwrite`.
"""

from __future__ import annotations
import argparse
import os
import sys
import time
from pathlib import Path
from typing import List
import subprocess
import shutil
import tempfile

try:
    import cairosvg
    CAIROSVG_AVAILABLE = True
except Exception:
    CAIROSVG_AVAILABLE = False

try:
    from PIL import Image, ImageOps
except Exception:
    print("Dependencia faltante: instala 'Pillow' (pip install pillow)")
    raise


DEFAULT_SIZES = [256, 128, 64, 48, 32, 16]


def backup_path(p: Path) -> Path:
    ts = int(time.time())
    return p.with_name(p.name + f".bak.{ts}")


def _render_svg_to_raw_png(svg_path: Path, raw_png_path: Path) -> None:
    # Prefer cairosvg when available
    if CAIROSVG_AVAILABLE:
        try:
            cairosvg.svg2png(url=str(svg_path), write_to=str(raw_png_path))
            return
        except Exception:
            # fallthrough to inkscape
            pass

    # Fallback: use inkscape CLI if available
    inkscape_cmd = shutil.which('inkscape')
    if not inkscape_cmd:
        raise RuntimeError('No renderer disponible: instala Cairo (para cairosvg) o Inkscape CLI')

    # Newer inkscape syntax (>=1.0): --export-type=png --export-filename=out.png
    # Older uses: -e out.png
    # Try modern syntax first
    modern_args = [inkscape_cmd, str(svg_path), '--export-type=png', f'--export-filename={str(raw_png_path)}']
    try:
        subprocess.run(modern_args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return
    except Exception:
        # try legacy
        legacy_args = [inkscape_cmd, '-z', str(svg_path), '-e', str(raw_png_path)]
        subprocess.run(legacy_args, check=True)
        return


def convert_svg_to_png(svg_path: Path, out_path: Path, size: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(suffix='.png', delete=False, dir=str(out_path.parent)) as tmp_file:
        raw_png_path = Path(tmp_file.name)

    try:
        _render_svg_to_raw_png(svg_path, raw_png_path)

        with Image.open(raw_png_path) as rendered:
            rendered = rendered.convert('RGBA')
            fitted = ImageOps.contain(rendered, (size, size), method=Image.LANCZOS)
            canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
            offset_x = (size - fitted.width) // 2
            offset_y = (size - fitted.height) // 2
            canvas.paste(fitted, (offset_x, offset_y), fitted)
            canvas.save(out_path)
    finally:
        try:
            raw_png_path.unlink(missing_ok=True)
        except Exception:
            pass


def make_pngs_for_svg(svg_path: Path, out_dir: Path, sizes: List[int], overwrite: bool, backup: bool) -> List[Path]:
    png_paths = []
    basename = svg_path.stem
    for s in sizes:
        png_name = f"{basename}-{s}.png"
        png_path = out_dir / png_name
        if png_path.exists():
            if not overwrite:
                print(f"Skipping existing {png_path} (use --overwrite to replace)")
                png_paths.append(png_path)
                continue
            if backup:
                b = backup_path(png_path)
                png_path.rename(b)
                print(f"Backup: {png_path} -> {b}")
        print(f"Rendering {svg_path.name} -> {png_path.name} ({s}x{s})")
        convert_svg_to_png(svg_path, png_path, s)
        png_paths.append(png_path)
    return png_paths


def create_ico_from_pngs(png_paths: List[Path], ico_path: Path, sizes: List[int], overwrite: bool, backup: bool) -> None:
    if ico_path.exists():
        if not overwrite:
            print(f"Skipping existing {ico_path} (use --overwrite to replace)")
            return
        if backup:
            b = backup_path(ico_path)
            ico_path.rename(b)
            print(f"Backup: {ico_path} -> {b}")

    # Load PNGs and save as multi-size ICO
    imgs = []
    for s in sizes:
        # find matching png
        candidates = [p for p in png_paths if p.stem.endswith(f"-{s}")]
        if not candidates:
            continue
        img = Image.open(candidates[0]).convert('RGBA')
        imgs.append(img)

    if not imgs:
        print("No PNGs available to build ICO. Abort.")
        return

    # Pillow accepts saving the largest image and passing sizes for additional icons
    largest = max(imgs, key=lambda im: im.size[0])
    ico_sizes = [(im.size[0], im.size[1]) for im in imgs]
    print(f"Saving ICO {ico_path} with sizes: {ico_sizes}")
    largest.save(ico_path, format='ICO', sizes=ico_sizes)


def find_svgs(folder: Path) -> List[Path]:
    return sorted([p for p in folder.iterdir() if p.suffix.lower() == '.svg'])


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', '-s', default='../../gfx/PrettySumatra', help='Carpeta fuente relativa al script (o ruta absoluta)')
    parser.add_argument('--out', '-o', default=None, help='Carpeta de salida (por defecto la misma carpeta de origen)')
    parser.add_argument('--sizes', '-z', default=','.join(map(str, DEFAULT_SIZES)), help='Tamaños separados por coma, por ejemplo 256,128,64')
    parser.add_argument('--ico', help='Nombre del SVG a usar para generar .ico (p.ej. SumatraPDF.svg). Si no se especifica, no se genera .ico')
    parser.add_argument('--overwrite', action='store_true', help='Sobrescribir archivos existentes')
    parser.add_argument('--no-backup', dest='backup', action='store_false', help='No crear copias de seguridad al sobrescribir')
    args = parser.parse_args(argv)

    src = Path(args.source).resolve()
    if not src.exists() or not src.is_dir():
        print(f"Carpeta fuente no encontrada: {src}")
        sys.exit(1)

    out_dir = Path(args.out).resolve() if args.out else src
    sizes = [int(x) for x in args.sizes.split(',') if x.strip()]

    svgs = find_svgs(src)
    if not svgs:
        print(f"No se encontraron SVGs en {src}")
        sys.exit(0)

    all_pngs = []
    for svg in svgs:
        try:
            pngs = make_pngs_for_svg(svg, out_dir, sizes, overwrite=args.overwrite, backup=args.backup)
            all_pngs.extend(pngs)
        except Exception as e:
            print(f"Error al procesar {svg}: {e}")

    if args.ico:
        ico_svg = src / args.ico
        if not ico_svg.exists():
            print(f"SVG para .ico no encontrado: {ico_svg}")
        else:
            # Ensure we have pngs for requested ico sizes
            ico_pngs = [p for p in all_pngs if p.stem.startswith(ico_svg.stem)]
            ico_path = out_dir / (ico_svg.stem + '.ico')
            create_ico_from_pngs(ico_pngs, ico_path, sizes, overwrite=args.overwrite, backup=args.backup)

    print("Hecho.")


if __name__ == '__main__':
    main()
