The branch named "main" is the upstream main branch where development occurs.

The branch name `2026.1` is the stable release this year, which means that new features from the main branch go here when they are considered sufficiently mature and stable.

The tag  `2026.1(TeXLive)` corresponds to the synctexdir directory of TeXLive svn repository from the 2026 release.

All other branches are secondary.

See synctex_version.h to see the latest version of synctex and of the parser.

# SyncTeX

Synchronization for TeX

The tagged history points are a partial clone of synctexdir in the TeXLive svn repository, see the online [TeXLive repository](http://www.tug.org/svn/texlive/trunk/Build/source/texk/web2c/synctexdir/).

The focus here is on the client side code for synchronization between text editor and pdf viewer. The files are not always forwarded to TeXLive as is.

Instructions for building and testing are available in [./meson/README.md](https://github.com/jlaurens/synctex/blob/main/meson/README.md).
