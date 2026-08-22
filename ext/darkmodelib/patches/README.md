# Our changes to darkmodelib, as patches

`ext/darkmodelib` is a vendored copy of [ozone10/darkmodelib](https://github.com/ozone10/darkmodelib)
that we edit in place. Each `.patch` is one logical change against the revision
in `ext/versions.txt`, in `git diff` format.

**Base revision: darkmodelib `0.75.0`** (commit `fa99647`). Paths in the patches
are relative to `ext/darkmodelib`, so `-p1` from inside that directory.

## The patches

| Patch | What |
| --- | --- |
| `0001-combo-lbox-dark-items-and-scrollbar` | ComboLBox items via `WM_CTLCOLORLISTBOX` and a painted dark scrollbar (#6000) |

Also kept in the tree, not as a delta patch:

- `include/DarkModeSubclass.h` — compatibility shim (`Darkmodelib.h` / `dmlib`, plus `WM_DPICHANGED_AFTERPARENT` for `WINVER=0x601`)
- `src/StdAfx.h` mingw DWM enums — Ubuntu 24.04's mingw-w64 11 headers predate those Windows 11 additions

## Applying them

```sh
# after copying a new darkmodelib revision over ext/darkmodelib, keep the shim
# and StdAfx.h mingw block, then:
cd ext/darkmodelib
git apply -p1 patches/0001-combo-lbox-dark-items-and-scrollbar.patch
```
