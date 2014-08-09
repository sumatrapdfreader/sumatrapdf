/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "CEpubFilter.h"
#include "EbookDoc.h"
#include "HtmlParserLookup.h"
#include "HtmlPullParser.h"
#include "WinUtil.h"

VOID CEpubFilter::CleanUp()
{
    if (m_epubDoc) {
        delete m_epubDoc;
        m_epubDoc = NULL;
    }
    m_state = STATE_EPUB_END;
}

HRESULT CEpubFilter::OnInit()
{
    CleanUp();

    // TODO: EpubDoc::CreateFromStream never returns with
    //       m_pStream instead of a clone - why?

    // load content of EPUB document into a seekable stream
    HRESULT res;
    size_t len;
    void *data = GetDataFromStream(m_pStream, &len, &res);
    if (!data)
        return res;

    ScopedComPtr<IStream> stream(CreateStreamFromData(data, len));
    free(data);
    if (!stream)
        return E_FAIL;

    m_epubDoc = EpubDoc::CreateFromStream(stream);
    if (!m_epubDoc)
        return E_FAIL;

    m_state = STATE_EPUB_START;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool IsoDateParse(const WCHAR *isoDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const WCHAR *end = str::Parse(isoDate, L"%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) // time is optional
        str::Parse(end, L"T%2d:%2d:%2dZ", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    return end != NULL;
    // don't bother about the day of week, we won't display it anyway
}

static WCHAR *ExtractHtmlText(EpubDoc *doc)
{
    size_t len;
    const char *data = doc->GetTextData(&len);

    str::Str<char> text(len / 2);
    HtmlPullParser p(data, len);
    HtmlToken *t;
    Vec<HtmlTag> tagNesting;
    while ((t = p.Next()) != NULL && !t->IsError()) {
        if (t->IsText() && !tagNesting.Contains(Tag_Head) && !tagNesting.Contains(Tag_Script) && !tagNesting.Contains(Tag_Style)) {
            // trim whitespace (TODO: also normalize within text?)
            while (t->sLen > 0 && str::IsWs(t->s[0])) {
                t->s++;
                t->sLen--;
            }
            while (t->sLen > 0 && str::IsWs(t->s[t->sLen-1]))
                t->sLen--;
            if (t->sLen > 0) {
                text.AppendAndFree(ResolveHtmlEntities(t->s, t->sLen));
                text.Append(' ');
            }
        }
        else if (t->IsStartTag()) {
            // TODO: force-close tags similar to HtmlFormatter.cpp's AutoCloseOnOpen?
            if (!IsTagSelfClosing(t->tag))
                tagNesting.Append(t->tag);
        }
        else if (t->IsEndTag()) {
            if (!IsInlineTag(t->tag) && text.Size() > 0 && text.Last() == ' ') {
                text.Pop();
                text.Append("\r\n");
            }
            // when closing a tag, if the top tag doesn't match but
            // there are only potentially self-closing tags on the
            // stack between the matching tag, we pop all of them
            if (tagNesting.Contains(t->tag)) {
                while (tagNesting.Last() != t->tag)
                    tagNesting.Pop();
            }
            if (tagNesting.Count() > 0 && tagNesting.Last() == t->tag)
                tagNesting.Pop();
        }
    }

    return str::conv::FromUtf8(text.Get());
}

HRESULT CEpubFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    ScopedMem<WCHAR> str;

    switch (m_state) {
    case STATE_EPUB_START:
        m_state = STATE_EPUB_AUTHOR;
        chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        return S_OK;

    case STATE_EPUB_AUTHOR:
        m_state = STATE_EPUB_TITLE;
        str.Set(m_epubDoc->GetProperty(Prop_Author));
        if (!str::IsEmpty(str.Get())) {
            chunkValue.SetTextValue(PKEY_Author, str);
            return S_OK;
        }
        // fall through

    case STATE_EPUB_TITLE:
        m_state = STATE_EPUB_DATE;
        str.Set(m_epubDoc->GetProperty(Prop_Title));
        if (!str) str.Set(m_epubDoc->GetProperty(Prop_Subject));
        if (!str::IsEmpty(str.Get())) {
            chunkValue.SetTextValue(PKEY_Title, str);
            return S_OK;
        }
        // fall through

    case STATE_EPUB_DATE:
        m_state = STATE_EPUB_CONTENT;
        str.Set(m_epubDoc->GetProperty(Prop_ModificationDate));
        if (!str) str.Set(m_epubDoc->GetProperty(Prop_CreationDate));
        if (!str::IsEmpty(str.Get())) {
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
        str.Set(ExtractHtmlText(m_epubDoc));
        if (!str::IsEmpty(str.Get())) {
            chunkValue.SetTextValue(PKEY_Search_Contents, str, CHUNK_TEXT);
            return S_OK;
        }
        // fall through

    case STATE_EPUB_END:
    default:
        return FILTER_E_END_OF_CHUNKS;
    }
}
