/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ChmEngine.h"
#include "DjVuEngine.h"
#include "Doc.h"
#include "EpubEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"

BaseEngine *Doc::AsEngine() const
{
    switch (type) {
    case None:
    case Mobi:
        return NULL;
    }
    return (BaseEngine*)pdfEngine;
}
