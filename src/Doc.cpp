/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"
#include "EngineManager.h"

BaseEngine *Doc::AsEngine() const
{
    switch (type) {
    case None:
    case Mobi:
        return NULL;
    }
    return pdfEngine;
}
