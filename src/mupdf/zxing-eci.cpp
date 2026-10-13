// zxing-cpp's ECI.cpp and zint's eci.c would both become eci.obj in the zxing
// static lib; building it under another name keeps the two apart. Same as
// mupdf's scripts/zxing-cpp/eci_dedup.cpp, with our include path.
#include "ECI.cpp"
