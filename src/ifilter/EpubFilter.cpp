/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Archive.h"
#include "base/HtmlTags.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "GumboHelpers.h"
#include "GumboHtmlParser.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "EpubFilter.h"

VOID EpubFilter::CleanUp() {
    log(StrL("EpubFilter::Cleanup()\n"));
    if (m_epubDoc) {
        delete m_epubDoc;
        m_epubDoc = nullptr;
    }
    m_state = STATE_EPUB_END;
}

HRESULT EpubFilter::OnInit() {
    log(StrL("EpubFilter::OnInit()\n"));

    CleanUp();

    if (str::IsNull(m_data)) {
        return E_FAIL;
    }
    m_epubDoc = EpubDoc::CreateFromData(m_data);
    if (!m_epubDoc) {
        return E_FAIL;
    }

    m_state = STATE_EPUB_START;
    return S_OK;
}

// copied from DocumentProperties.cpp
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
    str::TrimWsBoth(tokText);
}

static WStr ExtractHtmlText(EpubDoc* doc) {
    log(StrL("ExtractHtmlText()\n"));

    Str d = doc->GetHtmlData();
    int dataLen = d.len;

    str::Builder text;
    str::BuilderReserve(text, dataLen / 2);
    GumboHtmlParser p(d);
    HtmlToken* t;
    Vec<HtmlTag> tagNesting;
    while ((t = p.Next()) != nullptr && !t->IsError()) {
        if (t->IsText() && !VecContains(tagNesting, Tag_Head) && !VecContains(tagNesting, Tag_Script) &&
            !VecContains(tagNesting, Tag_Style)) {
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
                VecAppend(tagNesting, t->tag);
            }
        } else if (t->IsEndTag()) {
            if (!IsInlineTag(t->tag) && text.LastChar() == ' ') {
                text.RemoveLast();
                text.Append(StrL("\r\n"));
            }
            // when closing a tag, if the top tag doesn't match but
            // there are only potentially self-closing tags on the
            // stack between the matching tag, we pop all of them
            if (VecContains(tagNesting, t->tag)) {
                while (VecLast(tagNesting) != t->tag) {
                    VecPop(tagNesting);
                }
            }
            if (len(tagNesting) > 0 && VecLast(tagNesting) == t->tag) {
                VecPop(tagNesting);
            }
        }
    }

    return ToWStr(ToStr(text));
}

HRESULT EpubFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    log(StrL("EpubFilter::GetNextChunkValue()\n"));

    TempStr str;
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
            if (len(str) == 0) {
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
            if (len(str) == 0) {
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
