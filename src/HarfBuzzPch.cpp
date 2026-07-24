// This file exists only to generate the precompiled header for harfbuzz's
// hb.hh (configured via pchheader/pchsource in premake5.lua). Almost every
// harfbuzz .cc includes hb.hh (directly or via another header), and parsing
// it dominates harfbuzz compile time.
#include "hb.hh"
