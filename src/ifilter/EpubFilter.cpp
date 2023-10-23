/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "EpubFilter.h"

#include "utils/Log.h"

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
    HRESULT res;
    ByteSlice data = GetDataFromStream(m_pStream, &res);
    if (data.empty()) {
        return res;
    }

    IStream* strm = CreateStreamFromData(data);
    data.Free();
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
static bool IsoDateParse(const char* isoDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const char* end = str::Parse(isoDate, "%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) {
        // time is optional
        str::Parse(end, "T%2d:%2d:%2dZ", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    }
    return end != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static WCHAR* ExtractHtmlText(EpubDoc* doc) {
    log("ExtractHtmlText()\n");

    ByteSlice d = doc->GetHtmlData();
    size_t len = d.size();

    str::Str text(len / 2);
    HtmlPullParser p(d);
    HtmlToken* t;
    Vec<HtmlTag> tagNesting;
    while ((t = p.Next()) != nullptr && !t->IsError()) {
        if (t->IsText() && !tagNesting.Contains(Tag_Head) && !tagNesting.Contains(Tag_Script) &&
            !tagNesting.Contains(Tag_Style)) {
            // trim whitespace (TODO: also normalize within text?)
            while (t->sLen > 0 && str::IsWs(t->s[0])) {
                t->s++;
                t->sLen--;
            }
            while (t->sLen > 0 && str::IsWs(t->s[t->sLen - 1])) {
                t->sLen--;
            }
            if (t->sLen > 0) {
                text.AppendAndFree(ResolveHtmlEntities(t->s, t->sLen));
                text.AppendChar(' ');
            }
        } else if (t->IsStartTag()) {
            // TODO: force-close tags similar to HtmlFormatter.cpp's AutoCloseOnOpen?
            if (!IsTagSelfClosing(t->tag)) {
                tagNesting.Append(t->tag);
            }
        } else if (t->IsEndTag()) {
            if (!IsInlineTag(t->tag) && text.size() > 0 && text.Last() == ' ') {
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
            if (tagNesting.size() > 0 && tagNesting.Last() == t->tag) {
                tagNesting.Pop();
            }
        }
    }

    return ToWstr(text.Get());
}

HRESULT EpubFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    log("EpubFilter::GetNextChunkValue()\n");

    char* str = nullptr;
    WCHAR* ws = nullptr;

    switch (m_state) {
        case STATE_EPUB_START:
            m_state = STATE_EPUB_AUTHOR;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case STATE_EPUB_AUTHOR:
            m_state = STATE_EPUB_TITLE;
            str = m_epubDoc->GetProperty(DocumentProperty::Author);
            if (!str::IsEmpty(str)) {
                ws = ToWStrTemp(str);
                chunkValue.SetTextValue(PKEY_Author, ws);
                str::Free(str);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_TITLE:
            m_state = STATE_EPUB_DATE;
            str = m_epubDoc->GetProperty(DocumentProperty::Title);
            if (!str) {
                str::Free(str);
                str = m_epubDoc->GetProperty(DocumentProperty::Subject);
            }
            if (!str::IsEmpty(str)) {
                ws = ToWStrTemp(str);
                str::Free(str);
                chunkValue.SetTextValue(PKEY_Title, ws);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_DATE:
            m_state = STATE_EPUB_CONTENT;
            str = m_epubDoc->GetProperty(DocumentProperty::ModificationDate);
            if (!str) {
                str = m_epubDoc->GetProperty(DocumentProperty::CreationDate);
            }
            if (!str::IsEmpty(str)) {
                SYSTEMTIME systime;
                if (IsoDateParse(str, &systime)) {
                    str::Free(str);
                    FILETIME filetime;
                    SystemTimeToFileTime(&systime, &filetime);
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
                str::Free(str);
            }
            // fall through

        case STATE_EPUB_CONTENT:
            m_state = STATE_EPUB_END;
            ws = ExtractHtmlText(m_epubDoc);
            if (!str::IsEmpty(ws)) {
                chunkValue.SetTextValue(PKEY_Search_Contents, ws, CHUNK_TEXT);
                str::Free(ws);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_END:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
