/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/PlatformFont.h"
#include "linux/LinuxApp.h"

int main(int argc, char** argv) {
    int code = RunLinuxApp(argc, argv);
    PlatformFontShutdown();
    DestroyTempArena();
    DestroyPermArena();
    return code;
}
