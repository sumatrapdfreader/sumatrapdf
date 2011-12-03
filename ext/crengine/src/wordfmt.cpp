#include "../include/crsetup.h"
#include "../include/lvstring.h"
#include "../include/lvstream.h"
#include "../include/lvtinydom.h"

//#ifndef ENABLE_ANTIWORD
//#define ENABLE_ANTIWORD 1
//#endif


#if ENABLE_ANTIWORD==1
#include "../include/wordfmt.h"

#ifdef _WIN32
extern "C" {
	int strcasecmp(const char *s1, const char *s2) {
		return stricmp(s1,s2);
	}
//char	*optarg = NULL;
//	int	optind = 0;
}
#endif

#ifdef _DEBUG
#define TRACE(x, ...) CRLog::trace(x)
#else
#define TRACE(x, ...)
#endif

static ldomDocumentWriter * writer = NULL;
static ldomDocument * doc = NULL;
static int image_index = 0;
static bool inside_p = false;
static bool inside_table = false;
static int table_col_count = 0;
static int inside_list = 0; // 0=none, 1=ul, 2=ol
static int alignment = 0;
static bool inside_li = false;
static bool last_space_char = false;
static short	sLeftIndent = 0;	/* Left indentation in twips */
static short	sLeftIndent1 = 0;	/* First line left indentation in twips */
static short	sRightIndent = 0;	/* Right indentation in twips */
static int	    usBeforeIndent = 0;	/* Vertical indent before paragraph in twips */
static int	    usAfterIndent = 0;	/* Vertical indent after paragraph in twips */
//static ldomNode * body = NULL;
//static ldomNode * head = NULL;

// Antiword Output handling
extern "C" {
#include "antiword.h"
}

static conversion_type	eConversionType = conversion_unknown;
static encoding_type	eEncoding = encoding_neutral;

#define LFAIL(x) \
    if ((x)) crFatalError(1111, "assertion failed: " #x)


static lString16 picasToPercent( const lChar16 * prop, int p, int minvalue, int maxvalue ) {
    int identPercent = 100 * p / 5000;
    if ( identPercent>maxvalue )
        identPercent = maxvalue;
    if ( identPercent<minvalue )
        identPercent = minvalue;
	//if ( identPercent!=0 )
	return lString16(prop) + lString16::itoa(identPercent) + L"%; ";
	//return lString16::empty_str;
}

static lString16 picasToPx( const lChar16 * prop, int p, int minvalue, int maxvalue ) {
    int v = 600 * p / 5000;
    if ( v>maxvalue )
        v = maxvalue;
    if ( v<minvalue )
        v = minvalue;
	if ( v!=0 )
		return lString16(prop) + lString16::itoa(v) + L"px; ";
	return lString16::empty_str;
}

static lString16 fontSizeToPercent( const lChar16 * prop, int p, int minvalue, int maxvalue ) {
    int v = 100 * p / 20;
    if ( v>maxvalue )
        v = maxvalue;
    if ( v<minvalue )
        v = minvalue;
	if ( v!=0 )
		return lString16(prop) + lString16::itoa(v) + L"%; ";
	return lString16::empty_str;
}

static void setOptions() {
    options_type tOptions = {
        DEFAULT_SCREEN_WIDTH,
        conversion_xml,
        TRUE,
        TRUE,
        FALSE,
        encoding_utf_8,
        INT_MAX,
        INT_MAX,
        level_default,
    };

    //vGetOptions(&tOptions);
    vSetOptions(&tOptions);
}

/*
 * vPrologue1 - get options and call a specific initialization
 */
static void
vPrologue1(diagram_type *pDiag, const char *szTask, const char *szFilename)
{
    //options_type	tOptions;

    LFAIL(pDiag == NULL);
    LFAIL(szTask == NULL || szTask[0] == '\0');

    options_type tOptions;
    vGetOptions(&tOptions);

    eConversionType = tOptions.eConversionType;
    eEncoding = tOptions.eEncoding;

    TRACE("antiword::vPrologue1()");
    //vPrologueXML(pDiag, &tOptions);

    lString16 title("Word document");
    writer->OnTagOpen(NULL, L"?xml");
    writer->OnAttribute(NULL, L"version", L"1.0");
    writer->OnAttribute(NULL, L"encoding", L"utf-8");
    writer->OnEncoding(L"utf-8", NULL);
    writer->OnTagBody();
    writer->OnTagClose(NULL, L"?xml");
    writer->OnTagOpenNoAttr(NULL, L"FictionBook");
    // DESCRIPTION
    writer->OnTagOpenNoAttr(NULL, L"description");
    writer->OnTagOpenNoAttr(NULL, L"title-info");
    writer->OnTagOpenNoAttr(NULL, L"book-title");
    writer->OnText(title.c_str(), title.length(), 0);
    writer->OnTagClose(NULL, L"book-title");
    writer->OnTagOpenNoAttr(NULL, L"title-info");
    writer->OnTagClose(NULL, L"description");
    // BODY
    writer->OnTagOpenNoAttr(NULL, L"body");
} /* end of vPrologue1 */


/*
 * vEpilogue - clean up after everything is done
 */
static void
vEpilogue(diagram_type *pDiag)
{
    TRACE("antiword::vEpilogue()");
    //vEpilogueTXT(pDiag->pOutFile);
    //vEpilogueXML(pDiag);
    if ( inside_p )
        writer->OnTagClose(NULL, L"p");
    writer->OnTagClose(NULL, L"body");
} /* end of vEpilogue */

/*
 * vImagePrologue - perform image initialization
 */
void
vImagePrologue(diagram_type *pDiag, const imagedata_type *pImg)
{
    TRACE("antiword::vImagePrologue()");
    //vImageProloguePS(pDiag, pImg);
} /* end of vImagePrologue */

/*
 * vImageEpilogue - clean up an image
 */
void
vImageEpilogue(diagram_type *pDiag)
{
    TRACE("antiword::vImageEpilogue()");
    //vImageEpiloguePS(pDiag);
} /* end of vImageEpilogue */

/*
 * bAddDummyImage - add a dummy image
 *
 * return TRUE when successful, otherwise FALSE
 */
BOOL
bAddDummyImage(diagram_type *pDiag, const imagedata_type *pImg)
{
    TRACE("antiword::vImageEpilogue()");
    //return bAddDummyImagePS(pDiag, pImg);
	return FALSE;
} /* end of bAddDummyImage */

/*
 * pCreateDiagram - create and initialize a diagram
 *
 * remark: does not return if the diagram can't be created
 */
diagram_type *
pCreateDiagram(const char *szTask, const char *szFilename)
{
    TRACE("antiword::pCreateDiagram()");
    diagram_type	*pDiag;

    LFAIL(szTask == NULL || szTask[0] == '\0');

    /* Get the necessary memory */
    pDiag = (diagram_type *)xmalloc(sizeof(diagram_type));
    /* Initialization */
    pDiag->pOutFile = stdout;
    vPrologue1(pDiag, szTask, szFilename);
    /* Return success */
    return pDiag;
} /* end of pCreateDiagram */

/*
 * vDestroyDiagram - remove a diagram by freeing the memory it uses
 */
void
vDestroyDiagram(diagram_type *pDiag)
{
    TRACE("antiword::vDestroyDiagram()");

    LFAIL(pDiag == NULL);

    if (pDiag == NULL) {
        return;
    }
    vEpilogue(pDiag);
    pDiag = (diagram_type *)xfree(pDiag);
} /* end of vDestroyDiagram */

/*
 * vPrologue2 - call a specific initialization
 */
void
vPrologue2(diagram_type *pDiag, int iWordVersion)
{
    TRACE("antiword::vDestroyDiagram()");
//    vCreateBookIntro(pDiag, iWordVersion);
//    vCreateInfoDictionary(pDiag, iWordVersion);
//    vAddFontsPDF(pDiag);
} /* end of vPrologue2 */

/*
 * vMove2NextLine - move to the next line
 */
void
vMove2NextLine(diagram_type *pDiag, drawfile_fontref tFontRef,
    USHORT usFontSize)
{
    TRACE("antiword::vMove2NextLine()");
    LFAIL(pDiag == NULL);
    LFAIL(pDiag->pOutFile == NULL);
    LFAIL(usFontSize < MIN_FONT_SIZE || usFontSize > MAX_FONT_SIZE);

    if ( (inside_p || inside_li) && !last_space_char )
        writer->OnText(L" ", 1, 0);
    //writer->OnTagOpenAndClose(NULL, L"br");
    //vMove2NextLineXML(pDiag);
} /* end of vMove2NextLine */

/*
 * vSubstring2Diagram - put a sub string into a diagram
 */
void
vSubstring2Diagram(diagram_type *pDiag,
    char *szString, size_t tStringLength, long lStringWidth,
    UCHAR ucFontColor, USHORT usFontstyle, drawfile_fontref tFontRef,
    USHORT usFontSize, USHORT usMaxFontSize)
{
    lString16 s( szString, tStringLength);
#ifdef _LINUX
    TRACE("antiword::vSubstring2Diagram(%s)", LCSTR(s));
#else
    TRACE("antiword::vSubstring2Diagram()");
#endif
    s.trimDoubleSpaces(!last_space_char, true, false);
    last_space_char = (s.lastChar()==' ');
//    vSubstringXML(pDiag, szString, tStringLength, lStringWidth,
//            usFontstyle);
    if ( !inside_p && !inside_li ) {
        writer->OnTagOpenNoAttr(NULL, L"p");
        inside_p = true;
    }
    bool styleBold = bIsBold(usFontstyle);
    bool styleItalic = bIsItalic(usFontstyle);
    lString16 style;
	style << fontSizeToPercent( L"font-size: ", usFontSize, 30, 300 );
    if ( !style.empty() ) {
        writer->OnTagOpen(NULL, L"span");
        writer->OnAttribute(NULL, L"style", style.c_str());
        writer->OnTagBody();
    }
    if ( styleBold )
        writer->OnTagOpenNoAttr(NULL, L"b");
    if ( styleItalic )
        writer->OnTagOpenNoAttr(NULL, L"i");
    //=================
    writer->OnText(s.c_str(), s.length(), 0);
    //=================
    if ( styleItalic )
        writer->OnTagClose(NULL, L"i");
    if ( styleBold )
        writer->OnTagClose(NULL, L"b");
    if ( !style.empty() )
        writer->OnTagClose(NULL, L"span");

    pDiag->lXleft += lStringWidth;
} /* end of vSubstring2Diagram */

extern "C" {
    void vStoreStyle(diagram_type *pDiag, output_type *pOutput,
        const style_block_type *pStyle);
};

/*
 * vStoreStyle - store a style
 */
void
vStoreStyle(diagram_type *pDiag, output_type *pOutput,
    const style_block_type *pStyle)
{
    //size_t	tLen;
    //char	szString[120];

    LFAIL(pDiag == NULL);
    LFAIL(pOutput == NULL);
    LFAIL(pStyle == NULL);

    alignment = pStyle->ucAlignment;
    sLeftIndent = pStyle->sLeftIndent;	/* Left indentation in twips */
    sLeftIndent1 = pStyle->sLeftIndent1;	/* First line left indentation in twips */
    sRightIndent = pStyle->sRightIndent;	/* Right indentation in twips */
    usBeforeIndent = pStyle->usBeforeIndent;	/* Vertical indent before paragraph in twips */
    usAfterIndent = pStyle->usAfterIndent;	/* Vertical indent after paragraph in twips */

    TRACE("antiword::vStoreStyle(al=%d, li1=%d, li=%d, ri=%d)", alignment, sLeftIndent1, sLeftIndent, sRightIndent);
    //styleBold = pStyle->style_block_tag

} /* end of vStoreStyle */
/*
 * Create a start of paragraph (phase 1)
 * Before indentation, list numbering, bullets etc.
 */
void
vStartOfParagraph1(diagram_type *pDiag, long lBeforeIndentation)
{
    TRACE("antiword::vStartOfParagraph1()");
    LFAIL(pDiag == NULL);
    last_space_char = false;
} /* end of vStartOfParagraph1 */

/*
 * Create a start of paragraph (phase 2)
 * After indentation, list numbering, bullets etc.
 */
void
vStartOfParagraph2(diagram_type *pDiag)
{
    TRACE("antiword::vStartOfParagraph2()");
    LFAIL(pDiag == NULL);

    lString16 style;
    if ( !inside_p && !inside_list && !inside_li ) {
        writer->OnTagOpen(NULL, L"p");
        if ( alignment==ALIGNMENT_CENTER )
            style << L"text-align: center; ";
        else if ( alignment==ALIGNMENT_RIGHT )
            style << L"text-align: right; ";
        else if ( alignment==ALIGNMENT_JUSTIFY )
            style << L"text-align: justify; text-indent: 1.3em; ";
        else
            style << L"text-align: left; ";
        //if ( sLeftIndent1!=0 )
        //style << picasToPercent(L"text-indent: ", sLeftIndent1, 0, 20);
        if ( sLeftIndent!=0 )
            style << picasToPercent(L"margin-left: ", sLeftIndent, 0, 40);
        if ( sRightIndent!=0 )
            style << picasToPercent(L"margin-right: ", sRightIndent, 0, 30);
        if ( usBeforeIndent!=0 )
            style << picasToPx(L"margin-top: ", usBeforeIndent, 0, 20);
        if ( usAfterIndent!=0 )
            style << picasToPx(L"margin-bottom: ", usAfterIndent, 0, 20);
        if ( !style.empty() )
            writer->OnAttribute(NULL, L"style", style.c_str());
        writer->OnTagBody();
        inside_p = true;
    }
    //vStartOfParagraphXML(pDiag, 1);
} /* end of vStartOfParagraph2 */

/*
 * Create an end of paragraph
 */
void
vEndOfParagraph(diagram_type *pDiag,
    drawfile_fontref tFontRef, USHORT usFontSize, long lAfterIndentation)
{
    TRACE("antiword::vEndOfParagraph()");
    LFAIL(pDiag == NULL);
    LFAIL(pDiag->pOutFile == NULL);
    LFAIL(usFontSize < MIN_FONT_SIZE || usFontSize > MAX_FONT_SIZE);
    LFAIL(lAfterIndentation < 0);
    //vEndOfParagraphXML(pDiag, 1);
    if ( inside_p ) {
        writer->OnTagClose(NULL, L"p");
        inside_p = false;
    }
} /* end of vEndOfParagraph */

/*
 * Create an end of page
 */
void
vEndOfPage(diagram_type *pDiag, long lAfterIndentation, BOOL bNewSection)
{
    TRACE("antiword::vEndOfPage()");
    //vEndOfPageXML(pDiag);
} /* end of vEndOfPage */

/*
 * vSetHeaders - set the headers
 */
void
vSetHeaders(diagram_type *pDiag, USHORT usIstd)
{
    TRACE("antiword::vEndOfPage()");
    //vSetHeadersXML(pDiag, usIstd);
} /* end of vSetHeaders */

/*
 * Create a start of list
 */
void
vStartOfList(diagram_type *pDiag, UCHAR ucNFC, BOOL bIsEndOfTable)
{
    TRACE("antiword::vStartOfList()");

    if ( bIsEndOfTable!=0 )
        vEndOfTable(pDiag);

    if ( inside_list==0 ) {
        switch( ucNFC ) {
        case LIST_BULLETS:
            inside_list = 1;
            writer->OnTagOpenNoAttr(NULL, L"ul");
            break;
        default:
            inside_list = 2;
            writer->OnTagOpenNoAttr(NULL, L"ol");
            break;
        }
    }
    inside_li = false;

    //vStartOfListXML(pDiag, ucNFC, bIsEndOfTable);
} /* end of vStartOfList */

/*
 * Create an end of list
 */
void
vEndOfList(diagram_type *pDiag)
{
    TRACE("antiword::vEndOfList()");

    if ( inside_li ) {
        writer->OnTagClose(NULL, L"li");
        inside_li = false;
    }
    if ( inside_list==1 )
        writer->OnTagClose(NULL, L"ul");
    else if ( inside_list==2 )
        writer->OnTagClose(NULL, L"ol");

    //vEndOfListXML(pDiag);
} /* end of vEndOfList */

/*
 * Create a start of a list item
 */
void
vStartOfListItem(diagram_type *pDiag, BOOL bNoMarks)
{
    TRACE("antiword::vStartOfListItem()");
    if ( inside_li ) {
        writer->OnTagClose(NULL, L"li");
    }
    inside_li = true;
    writer->OnTagOpenNoAttr(NULL, L"li");
    //vStartOfListItemXML(pDiag, bNoMarks);
} /* end of vStartOfListItem */

/*
 * Create an end of a table
 */
void
vEndOfTable(diagram_type *pDiag)
{
    TRACE("antiword::vEndOfTable()");
    if ( inside_table ) {
        writer->OnTagClose(NULL, L"table");
        inside_table = false;
		table_col_count = 0;
    }
} /* end of vEndOfTable */

/*
 * Add a table row
 *
 * Returns TRUE when conversion type is XML
 */
BOOL
bAddTableRow(diagram_type *pDiag, char **aszColTxt,
    int iNbrOfColumns, const short *asColumnWidth, UCHAR ucBorderInfo)
{
    TRACE("antiword::bAddTableRow()");
//        vAddTableRowXML(pDiag, aszColTxt,
//                iNbrOfColumns, asColumnWidth,
//                ucBorderInfo);
	if ( table_col_count!=iNbrOfColumns ) {
		if (inside_table)
			writer->OnTagClose(NULL, L"table");
		writer->OnTagOpenNoAttr(NULL, L"table");
        inside_table = true;
		int totalWidth = 0;
		int i;
		for ( i=0; i<iNbrOfColumns; i++ )
			totalWidth += asColumnWidth[i];
		if ( totalWidth>0 ) {
			for ( i=0; i<iNbrOfColumns; i++ ) {
				int cw = asColumnWidth[i] * 100 / totalWidth;
		        writer->OnTagOpen(NULL, L"col");
				if ( cw>=0 )
					writer->OnAttribute(NULL, L"width", (lString16::itoa(cw) + L"%").c_str());
		        writer->OnTagBody();
		        writer->OnTagClose(NULL, L"col");
			}
		}
		table_col_count = iNbrOfColumns;
	}
    if (!inside_table) {
        writer->OnTagOpenNoAttr(NULL, L"table");
        inside_table = true;
    }
    writer->OnTagOpenNoAttr(NULL, L"tr");
    for ( int i=0; i<iNbrOfColumns; i++ ) {
        writer->OnTagOpenNoAttr(NULL, L"td");
        lString16 text = lString16(aszColTxt[i]);
        writer->OnText(text.c_str(), text.length(), 0);
        writer->OnTagClose(NULL, L"td");
    }
    writer->OnTagClose(NULL, L"tr");
    return TRUE;
    //return FALSE;
} /* end of bAddTableRow */


static LVStream * antiword_stream = NULL;
class AntiwordStreamGuard {
public:
    AntiwordStreamGuard(LVStreamRef stream) {
        antiword_stream = stream.get();
    }
    ~AntiwordStreamGuard() {
        antiword_stream = NULL;
    }
    operator FILE * () {
        return (FILE*)antiword_stream;
    }
};

void aw_rewind(FILE * pFile)
{
    if ( (void*)pFile==(void*)antiword_stream ) {
        antiword_stream->SetPos(0);
    } else {
        rewind(pFile);
    }
}

int aw_getc(FILE * pFile)
{
    if ( (void*)pFile==(void*)antiword_stream ) {
        int b = antiword_stream->ReadByte();
        if ( b>=0 )
            return b;
        return EOF;
    } else {
        return getc(pFile);
    }
}

/*
 * bReadBytes
 * This function reads the specified number of bytes from the specified file,
 * starting from the specified offset.
 * Returns TRUE when successfull, otherwise FALSE
 */
BOOL
bReadBytes(UCHAR *aucBytes, size_t tMemb, ULONG ulOffset, FILE *pFile)
{
    LFAIL(aucBytes == NULL || pFile == NULL || ulOffset > (ULONG)LONG_MAX);

    if ( (void*)pFile==(void*)antiword_stream ) {
        // use CoolReader stream
        LVStream * stream = (LVStream*)pFile;
        // default implementation from Antiword
        if (ulOffset > (ULONG)LONG_MAX) {
            return FALSE;
        }
        if (stream->SetPos(ulOffset)!=ulOffset ) {
            return FALSE;
        }
        lvsize_t bytesRead=0;
        if ( stream->Read(aucBytes, tMemb*sizeof(UCHAR), &bytesRead)!=LVERR_OK || bytesRead!=tMemb ) {
            return FALSE;
        }
    } else {
        // default implementation from Antiword
        if (ulOffset > (ULONG)LONG_MAX) {
            return FALSE;
        }
        if (fseek(pFile, (long)ulOffset, SEEK_SET) != 0) {
            return FALSE;
        }
        if (fread(aucBytes, sizeof(UCHAR), tMemb, pFile) != tMemb) {
            return FALSE;
        }
    }
    return TRUE;
} /* end of bReadBytes */

/*
 * bTranslateImage - translate the image
 *
 * This function reads the type of the given image and and gets it translated.
 *
 * return TRUE when sucessful, otherwise FALSE
 */
BOOL
bTranslateImage(diagram_type *pDiag, FILE *pFile, BOOL bMinimalInformation,
        ULONG ulFileOffsetImage, const imagedata_type *pImg)
{
    options_type    tOptions;

    DBG_MSG("bTranslateImage");

    fail(pDiag == NULL);
    fail(pFile == NULL);
    fail(ulFileOffsetImage == FC_INVALID);
    fail(pImg == NULL);
    fail(pImg->iHorSizeScaled <= 0);
    fail(pImg->iVerSizeScaled <= 0);

    vGetOptions(&tOptions);
    fail(tOptions.eImageLevel == level_no_images);

    if (bMinimalInformation) {
        return bAddDummyImage(pDiag, pImg);
    }

    switch (pImg->eImageType) {
    case imagetype_is_jpeg:
    case imagetype_is_png:
        {
            lUInt32 offset = ulFileOffsetImage + pImg->tPosition;
            lUInt32 len = pImg->tLength - pImg->tPosition;

            if (!bSetDataOffset(pFile, offset)) {
                return FALSE;
            }

            lUInt8 *pucJpeg, *pucTmp;
            size_t  tLen;
            int     iByte;

            pucJpeg = (lUInt8*)malloc(len);
            for (pucTmp = pucJpeg, tLen = 0; tLen < len; pucTmp++, tLen++) {
                iByte = iNextByte(pFile);
                if (iByte == EOF) {
                        return FALSE;
                }
                *pucTmp = (UCHAR)iByte;
            }

            // add Image BLOB
            lString16 name(BLOB_NAME_PREFIX); // L"@blob#"
            name << L"image";
            name << lString16::itoa(image_index++);
            name << (pImg->eImageType==imagetype_is_jpeg ? L".jpg" : L".png");
            writer->OnBlob(name, pucJpeg, len);
            writer->OnTagOpen(LXML_NS_NONE, L"img");
            writer->OnAttribute(LXML_NS_NONE, L"src", name.c_str());
            writer->OnTagClose(LXML_NS_NONE, L"img");

            free(pucJpeg);
            return TRUE;
        }
     case imagetype_is_dib:
     case imagetype_is_emf:
     case imagetype_is_wmf:
     case imagetype_is_pict:
     case imagetype_is_external:
         /* FIXME */
         return bAddDummyImage(pDiag, pImg);
     case imagetype_is_unknown:
     default:
         DBG_DEC(pImg->eImageType);
         return bAddDummyImage(pDiag, pImg);
     }
} /* end of bTranslateImage */


bool DetectWordFormat( LVStreamRef stream )
{
    AntiwordStreamGuard file(stream);

    setOptions();

    BOOL bResult = 0;
    lUInt32 lFilesize = (lUInt32)stream->GetSize();
    int iWordVersion = iGuessVersionNumber(file, lFilesize);
    if (iWordVersion < 0 || iWordVersion == 3) {
        if (bIsRtfFile(file)) {
            CRLog::error("not a Word Document."
                " It is probably a Rich Text Format file");
        } if (bIsWordPerfectFile(file)) {
            CRLog::error("not a Word Document."
                " It is probably a Word Perfect file");
        } else {
            CRLog::error("not a Word Document");
        }
        return FALSE;
    }
    return true;
}

bool ImportWordDocument( LVStreamRef stream, ldomDocument * m_doc, LVDocViewCallback * progressCallback, CacheLoadingCallback * formatCallback )
{
    AntiwordStreamGuard file(stream);

    setOptions();

	inside_p = false;
	inside_table = false;
	table_col_count = 0;
	inside_list = 0; // 0=none, 1=ul, 2=ol
	alignment = 0;
	inside_li = false;
    last_space_char = false;
	sLeftIndent = 0;	/* Left indentation in twips */
	sLeftIndent1 = 0;	/* First line left indentation in twips */
	sRightIndent = 0;	/* Right indentation in twips */
	usBeforeIndent = 0;	/* Vertical indent before paragraph in twips */
	usAfterIndent = 0;	/* Vertical indent after paragraph in twips */

    BOOL bResult = 0;
    diagram_type	*pDiag;
    int		iWordVersion;

    lUInt32 lFilesize = (lUInt32)stream->GetSize();
    iWordVersion = iGuessVersionNumber(file, lFilesize);
    if (iWordVersion < 0 || iWordVersion == 3) {
        if (bIsRtfFile(file)) {
            CRLog::error("not a Word Document."
                " It is probably a Rich Text Format file");
        } if (bIsWordPerfectFile(file)) {
            CRLog::error("not a Word Document."
                " It is probably a Word Perfect file");
        } else {
            CRLog::error("not a Word Document");
        }
        return FALSE;
    }
    /* Reset any reading done during file testing */
    stream->SetPos(0);


    ldomDocumentWriter w(m_doc);
    writer = &w;
    doc = m_doc;
    image_index = 0;



    pDiag = pCreateDiagram("cr3", "filename.doc");
    if (pDiag == NULL) {
        return false;
    }

    bResult = bWordDecryptor(file, lFilesize, pDiag);
    vDestroyDiagram(pDiag);

    doc = NULL;
    writer = NULL;

#ifdef _DEBUG
#define SAVE_COPY_OF_LOADED_DOCUMENT 1//def _DEBUG
#endif
    if ( bResult!=0 ) {
#ifdef SAVE_COPY_OF_LOADED_DOCUMENT //def _DEBUG
        LVStreamRef ostream = LVOpenFileStream( "/tmp/test_save_source.xml", LVOM_WRITE );
		if ( !ostream.isNull() )
			m_doc->saveToStream( ostream, "utf-16" );
#endif
    }

    return bResult!=0;
}


#endif //ENABLE_ANTIWORD==1


