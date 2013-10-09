This is a directory with exploratory lua tests. To execute you need luajit.

To set things up using binaries from https://code.google.com/p/lua-files project:
 * create lua directory somewhere and add it to %PATH%
 * from bin directory copy luajit.exe, luajit.exe.manifest, lua51.dll
   to lua directory
 * copy jit directory to lua directory

To run examples: luajit t00.lua

Currently winapi is taken from lua-files project because it already
implements a lot of stuff.

lua-files is in public domain.

The downside is that I don't understand the system and they've made
some architectural decisions (about e.g. object system) that might
not be the best. It'll do for now.
