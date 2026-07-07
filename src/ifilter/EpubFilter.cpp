/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Archive.h"
#include "base/HtmlTags.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "GumboHtmlParser.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "EpubFilter.h"

VOID EpubFilter::CleanUp() {
    log("EpubFilter::Cleanup()\n");
    if (m_epubDoc) {
        delete m_epubDoc;
        m_epubDoc = nullptr;
    }
    m_state = STATE_EPUB_END;
}

HRESULT EpubFilter::OnInit() {
    log("EpubFilter::OnInit()\n");

    CleanUp();

    // TODO: EpubDoc::CreateFromStream never returns with
    //       m_pStream instead of a clone - why?

    // load content of EPUB document into a seekable stream
    Str data = ReadIStream(m_pStream);
    if (str::IsNull(data)) {
        return E_FAIL;
    }

    IStream* strm = CreateStreamFromData(data);
    str::Free(data);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return E_FAIL;
    }

    m_epubDoc = EpubDoc::CreateFromStream(stream);
    if (!m_epubDoc) {
        return E_FAIL;
    }

    m_state = STATE_EPUB_START;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool IsoDateParse(Str isoDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    int year = 0, month = 0, day = 0;
    Str end = str::Parse(isoDate, "%4d-%2d-%2d", &year, &month, &day);
    if (end.s) {
        timeOut->wYear = (WORD)year;
        timeOut->wMonth = (WORD)month;
        timeOut->wDay = (WORD)day;
        // time is optional
        int hour = 0, minute = 0, second = 0;
        if (str::Parse(end, "T%2d:%2d:%2dZ", &hour, &minute, &second).s) {
            timeOut->wHour = (WORD)hour;
            timeOut->wMinute = (WORD)minute;
            timeOut->wSecond = (WORD)second;
        }
    }
    return end.s != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static void TrimHtmlTextToken(Str& tokText) {
    while (len(tokText) > 0 && str::IsWs(tokText.s[0])) {
        tokText.s++;
        tokText.len--;
    }
    while (len(tokText) > 0 && str::IsWs(tokText.s[tokText.len - 1])) {
        tokText.len--;
    }
}

static WStr ExtractHtmlText(EpubDoc* doc) {
    log("ExtractHtmlText()\n");

    Str d = doc->GetHtmlData();
    int dataLen = d.len;

    str::Builder text(dataLen / 2);
    GumboHtmlParser p(d);
    HtmlToken* t;
    Vec<HtmlTag> tagNesting;
    while ((t = p.Next()) != nullptr && !t->IsError()) {
        if (t->IsText() && !tagNesting.Contains(Tag_Head) && !tagNesting.Contains(Tag_Script) &&
            !tagNesting.Contains(Tag_Style)) {
            // trim whitespace (TODO: also normalize within text?)
            Str tokText = t->s;
            TrimHtmlTextToken(tokText);
            if (len(tokText) > 0) {
                TempStr s = ResolveHtmlEntitiesTemp(tokText);
                text.Append(s);
                text.AppendChar(' ');
            }
        } else if (t->IsStartTag()) {
            // TODO: force-close tags similar to HtmlFormatter.cpp's AutoCloseOnOpen?
            if (!IsTagSelfClosing(t->tag)) {
                tagNesting.Append(t->tag);
            }
        } else if (t->IsEndTag()) {
            if (!IsInlineTag(t->tag) && len(text) > 0 && text.Last() == ' ') {
                text.RemoveLast();
                text.Append("\r\n");
            }
            // when closing a tag, if the top tag doesn't match but
            // there are only potentially self-closing tags on the
            // stack between the matching tag, we pop all of them
            if (tagNesting.Contains(t->tag)) {
                while (tagNesting.Last() != t->tag) {
                    tagNesting.Pop();
                }
            }
            if (len(tagNesting) > 0 && tagNesting.Last() == t->tag) {
                tagNesting.Pop();
            }
        }
    }

    return ToWStr(ToStr(text));
}

HRESULT EpubFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    log("EpubFilter::GetNextChunkValue()\n");

    TempStr str = nullptr;
    WStr ws;

    switch (m_state) {
        case STATE_EPUB_START:
            m_state = STATE_EPUB_AUTHOR;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case STATE_EPUB_AUTHOR:
            m_state = STATE_EPUB_TITLE;
            str = m_epubDoc->GetPropertyTemp(DocProp::Author);
            if (len(str) > 0) {
                ws = ToWStrTemp(str);
                chunkValue.SetTextValue(PKEY_Author, ws.s);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_TITLE:
            m_state = STATE_EPUB_DATE;
            str = m_epubDoc->GetPropertyTemp(DocProp::Title);
            if (!str) {
                str = m_epubDoc->GetPropertyTemp(DocProp::Subject);
            }
            if (len(str) > 0) {
                ws = ToWStrTemp(str);
                chunkValue.SetTextValue(PKEY_Title, ws.s);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_DATE:
            m_state = STATE_EPUB_CONTENT;
            str = m_epubDoc->GetPropertyTemp(DocProp::ModificationDate);
            if (!str) {
                str = m_epubDoc->GetPropertyTemp(DocProp::CreationDate);
            }
            if (len(str) > 0) {
                SYSTEMTIME systime;
                if (IsoDateParse(str, &systime)) {
                    FILETIME filetime;
                    SystemTimeToFileTime(&systime, &filetime);
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
            }
            // fall through

        case STATE_EPUB_CONTENT:
            m_state = STATE_EPUB_END;
            ws = ExtractHtmlText(m_epubDoc);
            if (len(ws) > 0) {
                chunkValue.SetTextValue(PKEY_Search_Contents, ws.s, CHUNK_TEXT);
                wstr::Free(ws);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_END:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
