/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"

struct EBookUI;
EBookUI* GetEBookUI() {
    return nullptr;
}

struct FileEBookUI;
FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

VOID PdfFilter::CleanUp() {
    logf("PdfFilter::Cleanup()\n");
    if (m_pdfEngine) {
        m_pdfEngine->Release();
        m_pdfEngine = nullptr;
    }
    m_state = PdfFilterState::End;
}

HRESULT PdfFilter::OnInit() {
    logf("PdfFilter::OnInit()\n");
    CleanUp();

    if (str::IsNull(m_data)) {
        return E_FAIL;
    }
    m_pdfEngine = CreateEngineMupdfFromData(m_data, StrL("foo.pdf"), nullptr);
    if (!m_pdfEngine) {
        return E_FAIL;
    }

    m_state = PdfFilterState::Start;
    m_iPageNo = 0;
    return S_OK;
}

// copied from DocumentProperties.cpp
static bool PdfDateParse(Str pdfDate, SYSTEMTIME* timeOut) {
    if (len(pdfDate) == 0) {
        return false;
    }
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    Str slice = pdfDate;
    // "D:" at the beginning is optional
    str::TrimPrefix(slice, StrL("D:"));
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    Str end = str::Parse(slice,
                         "%4d%2d%2d"
                         "%2d%2d%2d",
                         &year, &month, &day, &hour, &minute, &second);
    if (str::IsNull(end)) {
        return false;
    }
    timeOut->wYear = (WORD)year;
    timeOut->wMonth = (WORD)month;
    timeOut->wDay = (WORD)day;
    timeOut->wHour = (WORD)hour;
    timeOut->wMinute = (WORD)minute;
    timeOut->wSecond = (WORD)second;
    return true;
    // don't bother about the day of week, we won't display it anyway
}

// Start, Author, Title, Date, Content, End

static Str PdfFilterStateToStr(PdfFilterState state) {
    Str res = SeqStrByIndex(kPdfFilterStateStrs, (int)state);
    return res ? res : StrL("unknown");
}

HRESULT PdfFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    Str stateStr = PdfFilterStateToStr(m_state);
    logf("PdfFilter::GetNextChunkValue(), state: %s (%d)\n", stateStr, (int)m_state);
    Str prop;
    WStr ws;
    switch (m_state) {
        case PdfFilterState::Start:
            m_state = PdfFilterState::Author;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case PdfFilterState::Author:
            m_state = PdfFilterState::Title;
            prop = m_pdfEngine->GetPropertyTemp(DocProp::Author);
            if (len(prop) > 0) {
                ws = ToWStrTemp(prop);
                chunkValue.SetTextValue(PKEY_Author, ws.s);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Title:
            m_state = PdfFilterState::Date;
            prop = m_pdfEngine->GetPropertyTemp(DocProp::Title);
            if (len(prop) == 0) {
                prop = m_pdfEngine->GetPropertyTemp(DocProp::Subject);
            }
            if (len(prop) > 0) {
                ws = ToWStrTemp(prop);
                chunkValue.SetTextValue(PKEY_Title, ws.s);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Date:
            m_state = PdfFilterState::Content;
            prop = m_pdfEngine->GetPropertyTemp(DocProp::ModificationDate);
            if (len(prop) == 0) {
                prop = m_pdfEngine->GetPropertyTemp(DocProp::CreationDate);
            }
            if (len(prop) > 0) {
                SYSTEMTIME systime;
                FILETIME filetime;
                if (PdfDateParse(prop, &systime) && SystemTimeToFileTime(&systime, &filetime)) {
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
            }

            [[fallthrough]];

        case PdfFilterState::Content:
            while (++m_iPageNo <= m_pdfEngine->PageCount()) {
                PageText pageText = m_pdfEngine->ExtractPageText(m_iPageNo);
                if (len(pageText.text) == 0) {
                    FreePageText(&pageText);
                    continue;
                }
                // IFilter text is CRLF; extraction uses \n. CHUNK_EOP: each
                // page is its own paragraph so the indexer doesn't glue pages
                // together (#4859).
                TempStr crlfText = str::ReplaceTemp(pageText.text, StrL("\n"), StrL("\r\n"));
                TempWStr text = ToWStrTemp(crlfText);
                chunkValue.SetTextValue(PKEY_Search_Contents, text.s, CHUNK_TEXT, 0, 0, 0, CHUNK_EOP);
                FreePageText(&pageText);
                return S_OK;
            }
            m_state = PdfFilterState::End;

            [[fallthrough]];

        case PdfFilterState::End:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
