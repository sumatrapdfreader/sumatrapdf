#include "base/Base.h"

// base expects log() from the host app; console tools print to stderr
void log(Str s) {
    if (len(s) == 0) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}
