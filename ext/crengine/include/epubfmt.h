#ifndef EPUBFMT_H
#define EPUBFMT_H

#include "../include/crsetup.h"
#include "../include/lvtinydom.h"


bool DetectEpubFormat( LVStreamRef stream );
bool ImportEpubDocument( LVStreamRef stream, ldomDocument * doc, LVDocViewCallback * progressCallback, CacheLoadingCallback * formatCallback );
lString16 EpubGetRootFilePath( LVContainerRef m_arc );


#endif // EPUBFMT_H
