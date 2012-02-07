/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mag_h
#define Mag_h

/*
Mag stands for "minimal graphics abstraction".

The ultimate goal is to be able to abstract between Gdi+ and DirectDraw
in Mui and possibly other places. The right one needs to be selected
at runtime. They provide very similar functionality but DirectDraw is
said to be much faster so it would be nice to use it on systems that
support it.

It's not namespaced because the names will be prefixed with "Mag" anyway
to avoid conflicts with existing names i.e. MagFont is beter than
mag::Font because it will never conflict with Gdiplus::Font.

Note: maybe it should live inside mui, not utils.

TODO: choose the right way to abstract things, with minimal overhead.
The obvious way is to use virtual functions for everything, expose
e.g. MuiGraphics abstract base class and have MuiGraphicsGdi, 
MuiGraphicsDirectDraw etc.

Less obvious but probably more compact (in lines of code) way is to have
a wrapper class MuiGraphics that embeds either Gdiplus::Graphics or its 
DirectDraw equivalent and redirects the calls to apropriate object based
on a global flag. The crucial point here is that we can tightly couple the
implementation (it'll result in smaller code) and the decision is made
globally, not on a per-object basis. Something like this:

class MuiGraphics {
  union {
      Gdiplus::Graphics *gdi;
      MadeUpDirectDrawGraphics *dd;
  };

  MuiGraphics *DrawRectangle(...) {
    if (gdi) return gdi->DrawRectangle(...);
    return dd->DrawRectangle(...);
  }

TODO: implement me.
*/

#endif
