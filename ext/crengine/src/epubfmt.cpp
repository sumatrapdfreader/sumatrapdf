#include "../include/epubfmt.h"


class EpubItem {
public:
    lString16 href;
    lString16 mediaType;
    lString16 id;
    lString16 title;
    EpubItem()
    { }
    EpubItem( const EpubItem & v )
        : href(v.href), mediaType(v.mediaType), id(v.id)
    { }
    EpubItem & operator = ( const EpubItem & v )
    {
        href = v.href;
        mediaType = v.mediaType;
        id = v.id;
        return *this;
    }
};

class EpubItems : public LVPtrVector<EpubItem> {
public:
    EpubItem * findById( const lString16 & id )
    {
        if ( id.empty() )
            return NULL;
        for ( int i=0; i<length(); i++ )
            if ( get(i)->id == id )
                return get(i);
        return NULL;
    }
};

static void dumpZip( LVContainerRef arc ) {
    lString16 arcName = LVExtractFilenameWithoutExtension( arc->GetName() );
    if ( arcName.empty() )
        arcName = L"unziparc";
    lString16 outDir = lString16("/tmp/") + arcName;
    LVCreateDirectory(outDir);
    for ( int i=0; i<arc->GetObjectCount(); i++ ) {
        const LVContainerItemInfo * info = arc->GetObjectInfo(i);
        if ( !info->IsContainer() ) {
            lString16 outFileName = outDir + L"/" + info->GetName();
            LVCreateDirectory(LVExtractPath(outFileName));
            LVStreamRef in = arc->OpenStream(info->GetName(), LVOM_READ);
            LVStreamRef out = LVOpenFileStream(outFileName.c_str(), LVOM_WRITE);
            if ( !in.isNull() && !out.isNull() ) {
                CRLog::trace("Writing %s", LCSTR(outFileName));
                LVPumpStream(out.get(), in.get());
            }
        }
    }
}

bool DetectEpubFormat( LVStreamRef stream )
{


    LVContainerRef m_arc = LVOpenArchieve( stream );
    if ( m_arc.isNull() )
        return false; // not a ZIP archive

    //dumpZip( m_arc );

    // read "mimetype" file contents from root of archive
    lString16 mimeType;
    {
        LVStreamRef mtStream = m_arc->OpenStream(L"mimetype", LVOM_READ );
        if ( !mtStream.isNull() ) {
            int size = mtStream->GetSize();
            if ( size>4 && size<100 ) {
                LVArray<char> buf( size+1, '\0' );
                if ( mtStream->Read( buf.get(), size, NULL )==LVERR_OK ) {
                    for ( int i=0; i<size; i++ )
                        if ( buf[i]<32 || ((unsigned char)buf[i])>127 )
                            buf[i] = 0;
                    buf[size] = 0;
                    if ( buf[0] )
                        mimeType = Utf8ToUnicode( lString8( buf.get() ) );
                }
            }
        }
    }

    if ( mimeType != L"application/epub+zip" )
        return false;
    return true;
}

void ReadEpubToc( ldomDocument * doc, ldomNode * mapRoot, LVTocItem * baseToc, ldomDocumentFragmentWriter & appender ) {
    if ( !mapRoot || !baseToc)
        return;
    lUInt16 navPoint_id = mapRoot->getDocument()->getElementNameIndex(L"navPoint");
    lUInt16 navLabel_id = mapRoot->getDocument()->getElementNameIndex(L"navLabel");
    lUInt16 content_id = mapRoot->getDocument()->getElementNameIndex(L"content");
    lUInt16 text_id = mapRoot->getDocument()->getElementNameIndex(L"text");
    for ( int i=0; i<5000; i++ ) {
        ldomNode * navPoint = mapRoot->findChildElement(LXML_NS_ANY, navPoint_id, i);
        if ( !navPoint )
            break;
        ldomNode * navLabel = navPoint->findChildElement(LXML_NS_ANY, navLabel_id, -1);
        if ( !navLabel )
            continue;
        ldomNode * text = navLabel->findChildElement(LXML_NS_ANY, text_id, -1);
        if ( !text )
            continue;
        ldomNode * content = navPoint->findChildElement(LXML_NS_ANY, content_id, -1);
        if ( !content )
            continue;
        lString16 href = content->getAttributeValue(L"src");
        lString16 title = text->getText(' ');
        title.trimDoubleSpaces(false, false, false);
        if ( href.empty() || title.empty() )
            continue;
        //CRLog::trace("TOC href before convert: %s", LCSTR(href));
        href = appender.convertHref(href);
        //CRLog::trace("TOC href after convert: %s", LCSTR(href));
        if ( href.empty() || href[0]!='#' )
            continue;
        ldomNode * target = doc->getNodeById(doc->getAttrValueIndex(href.substr(1).c_str()));
        if ( !target )
            continue;
        ldomXPointer ptr(target, 0);
        LVTocItem * tocItem = baseToc->addChild(title, ptr, lString16());
        ReadEpubToc( doc, navPoint, tocItem, appender );
    }
}

lString16 EpubGetRootFilePath( LVContainerRef m_arc )
{
    // check root media type
    lString16 rootfilePath;
    lString16 rootfileMediaType;
    // read container.xml
    {
        LVStreamRef container_stream = m_arc->OpenStream(L"META-INF/container.xml", LVOM_READ);
        if ( !container_stream.isNull() ) {
            ldomDocument * doc = LVParseXMLStream( container_stream );
            if ( doc ) {
                ldomNode * rootfile = doc->nodeFromXPath( lString16(L"container/rootfiles/rootfile") );
                if ( rootfile && rootfile->isElement() ) {
                    rootfilePath = rootfile->getAttributeValue(L"full-path");
                    rootfileMediaType = rootfile->getAttributeValue(L"media-type");
                }
                delete doc;
            }
        }
    }

    if ( rootfilePath.empty() || rootfileMediaType!=L"application/oebps-package+xml" )
        return lString16::empty_str;
    return rootfilePath;
}

bool ImportEpubDocument( LVStreamRef stream, ldomDocument * m_doc, LVDocViewCallback * progressCallback, CacheLoadingCallback * formatCallback )
{
    LVContainerRef m_arc = LVOpenArchieve( stream );
    if ( m_arc.isNull() )
        return false; // not a ZIP archive

    // check root media type
    lString16 rootfilePath = EpubGetRootFilePath(m_arc);
    if ( rootfilePath.empty() )
    	return false;

    m_doc->setContainer(m_arc);

    // read content.opf
    EpubItems epubItems;
    //EpubItem * epubToc = NULL; //TODO
    LVArray<EpubItem*> spineItems;
    lString16 codeBase;
    //lString16 css;

    //
    {
        codeBase=LVExtractPath(rootfilePath, false);
        CRLog::trace("codeBase=%s", LCSTR(codeBase));
    }

    LVStreamRef content_stream = m_arc->OpenStream(rootfilePath.c_str(), LVOM_READ);
    if ( content_stream.isNull() )
        return false;


    lString16 ncxHref;
    lString16 coverId;

    // reading content stream
    {
        ldomDocument * doc = LVParseXMLStream( content_stream );
        if ( !doc )
            return false;

        CRPropRef m_doc_props = m_doc->getProps();
        lString16 author = doc->textFromXPath( lString16(L"package/metadata/creator"));
        lString16 title = doc->textFromXPath( lString16(L"package/metadata/title"));
        m_doc_props->setString(DOC_PROP_TITLE, title);
        m_doc_props->setString(DOC_PROP_AUTHORS, author );
        CRLog::info("Author: %s Title: %s", LCSTR(author), LCSTR(title));
        for ( int i=1; i<20; i++ ) {
            ldomNode * item = doc->nodeFromXPath( lString16(L"package/metadata/meta[") + lString16::itoa(i) + L"]" );
            if ( !item )
                break;
            lString16 name = item->getAttributeValue(L"name");
            lString16 content = item->getAttributeValue(L"content");
            if ( name == L"cover" )
                coverId = content;
            else if ( name==L"calibre:series" )
                m_doc_props->setString(DOC_PROP_SERIES_NAME, content );
            else if ( name==L"calibre:series_index" )
                m_doc_props->setInt(DOC_PROP_SERIES_NUMBER, content.atoi() );
        }

        // items
        for ( int i=1; i<50000; i++ ) {
            ldomNode * item = doc->nodeFromXPath( lString16(L"package/manifest/item[") + lString16::itoa(i) + L"]" );
            if ( !item )
                break;
            lString16 href = item->getAttributeValue(L"href");
            lString16 mediaType = item->getAttributeValue(L"media-type");
            lString16 id = item->getAttributeValue(L"id");
            if ( !href.empty() && !id.empty() ) {
                if ( id==coverId ) {
                    // coverpage file
                    lString16 coverFileName = codeBase + href;
                    CRLog::info("EPUB coverpage file: %s", LCSTR(coverFileName));
                    LVStreamRef stream = m_arc->OpenStream(coverFileName.c_str(), LVOM_READ);
                    if ( !stream.isNull() ) {
                        LVImageSourceRef img = LVCreateStreamImageSource(stream);
                        if ( !img.isNull() ) {
                            CRLog::info("EPUB coverpage image is correct: %d x %d", img->GetWidth(), img->GetHeight() );
                            m_doc_props->setString(DOC_PROP_COVER_FILE, coverFileName);
                        }
                    }
                }
                EpubItem * epubItem = new EpubItem;
                epubItem->href = href;
                epubItem->id = id;
                epubItem->mediaType = mediaType;
                epubItems.add( epubItem );
            }
//            if ( mediaType==L"text/css" ) {
//                lString16 name = codeBase + href;
//                LVStreamRef cssStream = m_arc->OpenStream(name.c_str(), LVOM_READ);
//                if ( !cssStream.isNull() ) {
//                    css << L"\n";
//                    css << LVReadTextFile( cssStream );
//                }
//            }
        }

        // spine == itemrefs
        if ( epubItems.length()>0 ) {
            ldomNode * spine = doc->nodeFromXPath( lString16(L"package/spine") );
            if ( spine ) {

                EpubItem * ncx = epubItems.findById( spine->getAttributeValue(L"toc") ); //TODO
                //EpubItem * ncx = epubItems.findById(lString16("ncx"));
                if ( ncx!=NULL )
                    ncxHref = codeBase + ncx->href;

                for ( int i=1; i<50000; i++ ) {
                    ldomNode * item = doc->nodeFromXPath( lString16(L"package/spine/itemref[") + lString16::itoa(i) + L"]" );
                    if ( !item )
                        break;
                    EpubItem * epubItem = epubItems.findById( item->getAttributeValue(L"idref") );
                    if ( epubItem ) {
                        // TODO: add to document
                        spineItems.add( epubItem );
                    }
                }
            }
        }
        delete doc;
    }

    if ( spineItems.length()==0 )
        return false;


#if BUILD_LITE!=1
    if ( m_doc->openFromCache(formatCallback) ) {
        if ( progressCallback ) {
            progressCallback->OnLoadFileEnd( );
        }
        return true;
    }
#endif

    lUInt32 saveFlags = m_doc->getDocFlags();
    m_doc->setDocFlags( saveFlags );
    m_doc->setContainer( m_arc );

    ldomDocumentWriter writer(m_doc);
#if 0
    m_doc->setNodeTypes( fb2_elem_table );
    m_doc->setAttributeTypes( fb2_attr_table );
    m_doc->setNameSpaceTypes( fb2_ns_table );
#endif
    //m_doc->setCodeBase( codeBase );

    ldomDocumentFragmentWriter appender(&writer, lString16(L"body"), lString16(L"DocFragment"), lString16::empty_str );
    writer.OnStart(NULL);
    writer.OnTagOpenNoAttr(L"", L"body");
    int fragmentCount = 0;
    for ( int i=0; i<spineItems.length(); i++ ) {
        if ( spineItems[i]->mediaType==L"application/xhtml+xml" ) {
            lString16 name = codeBase + spineItems[i]->href;
            appender.addPathSubstitution( name, lString16(L"_doc_fragment_") + lString16::itoa(i) );
        }
    }
    for ( int i=0; i<spineItems.length(); i++ ) {
        if ( spineItems[i]->mediaType==L"application/xhtml+xml" ) {
            lString16 name = codeBase + spineItems[i]->href;
            {
                CRLog::debug("Checking fragment: %s", LCSTR(name));
                LVStreamRef stream = m_arc->OpenStream(name.c_str(), LVOM_READ);
                if ( !stream.isNull() ) {
                    appender.setCodeBase( name );
                    //LVXMLParser
                    LVHTMLParser parser(stream, &appender);
                    if ( parser.CheckFormat() && parser.Parse() ) {
                        // valid
                        fragmentCount++;
                    } else {
                        CRLog::error("Document type is not XML/XHTML for fragment %s", LCSTR(name));
                    }
                }
            }
        }
    }

    ldomDocument * ncxdoc = NULL;
    if ( !ncxHref.empty() ) {
        LVStreamRef stream = m_arc->OpenStream(ncxHref.c_str(), LVOM_READ);
        lString16 codeBase = LVExtractPath( ncxHref );
        if ( codeBase.length()>0 && codeBase.lastChar()!='/' )
            codeBase.append(1, L'/');
        appender.setCodeBase(codeBase);
        if ( !stream.isNull() ) {
            ldomDocument * ncxdoc = LVParseXMLStream( stream );
            if ( ncxdoc!=NULL ) {
                ldomNode * navMap = ncxdoc->nodeFromXPath( lString16(L"ncx/navMap"));
                if ( navMap!=NULL )
                    ReadEpubToc( m_doc, navMap, m_doc->getToc(), appender );
                delete ncxdoc;
            }
        }
    }

    writer.OnTagClose(L"", L"body");
    writer.OnStop();
    CRLog::debug("EPUB: %d documents merged", fragmentCount);

    if ( fragmentCount==0 )
        return false;

#if 0
    // set stylesheet
    //m_doc->getStyleSheet()->clear();
    m_doc->setStyleSheet( NULL, true );
    //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
    if ( !css.empty() && m_doc->getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {

        m_doc->setStyleSheet( "p.p { text-align: justify }\n"
            "svg { text-align: center }\n"
            "i { display: inline; font-style: italic }\n"
            "b { display: inline; font-weight: bold }\n"
            "abbr { display: inline }\n"
            "acronym { display: inline }\n"
            "address { display: inline }\n"
            "p.title-p { hyphenate: none }\n"
//abbr, acronym, address, blockquote, br, cite, code, dfn, div, em, h1, h2, h3, h4, h5, h6, kbd, p, pre, q, samp, span, strong, var
        , false);
        m_doc->setStyleSheet( UnicodeToUtf8(css).c_str(), false );
        //m_doc->getStyleSheet()->parse(UnicodeToUtf8(css).c_str());
    } else {
        //m_doc->getStyleSheet()->parse(m_stylesheet.c_str());
        //m_doc->setStyleSheet( m_stylesheet.c_str(), false );
    }
#endif
#if 0
    LVStreamRef out = LVOpenFileStream( L"c:\\doc.xml" , LVOM_WRITE );
    if ( !out.isNull() )
        m_doc->saveToStream( out, "utf-8" );
#endif

    // DONE!
    if ( progressCallback ) {
        progressCallback->OnLoadFileEnd( );
        m_doc->compact();
        m_doc->dumpStatistics();
    }
    return true;

}
