#pragma once

#include "resource.h"

// this is new C++ raw string. It does seem to mess up line numbers in compiler warnings,
// which is why it's not in the .cpp file
#define SAMPLE_TEXT "This program tests various ways of measuring/drawing text.\n" \
    "\n" \
    "The GDI+ method we currently use in ebook mode in Sumatra seems slow, which makes\n" \
    "breaking up into pages quite slow.\n" \
    "\n" \
    "According to http://theartofdev.wordpress.com/2013/08/12/the-wonders-of-text-rendering-and-gdi/\n" \
    "GDI can be substantially faster.\n" \
    "\n" \
    "Another option available under Vista (and later) is to use Direct Draw 2D.\n" \
    "\n" \
    "This program uses latest C++ features available in Visual Studio 2013, because I'm curious\n" \
    "if they provide substantial benefits.\n" \
    "\n" \
    "As a result, this code might not compile under earlier version of Visual Studio (I'm using\n" \
    "Visual Studio 2013 Update 1 to be exact).\n" \
    "\n" \
    "This program is also a simple text viewer, because it can only display text. For simplicity\n" \
    "it doesn't handle any markup.\n" \
    "\n" \
    "I have a need to convert Pixels to Points in C#. I've seen some complicated explanations about the topic, but can't seem to locate a simple formula. Let's assume a standard 96dpi, how do I calulate this conversion?\n" \
    "\n" \
    "System.Drawing.Graphics has DpiX and DpiY properties.DpiX is pixels per inch horizontally. DpiY is pixels per inch vertically. Use those to convert from points(72 points per inch) to pixels.\n"
