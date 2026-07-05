/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"


struct EBookUI;
EBookUI* GetEBookUI() {
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

    // TODO: EngineMupdf::CreateFromStream never returns with
    //       m_pStream instead of a clone - why?

    // load content of PDF document into a seekable stream
    HRESULT res;
    Str data = GetDataFromStream(m_pStream, &res);
    if (len(data) == 0) {
        return res;
    }

    IStream* strm = CreateStreamFromData(data);
    str::Free(data);
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return E_FAIL;
    }

    m_pdfEngine = CreateEngineMupdfFromStream(stream, "foo.pdf");
    if (!m_pdfEngine) {
        return E_FAIL;
    }

    m_state = PdfFilterState::Start;
    m_iPageNo = 0;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool PdfDateParse(Str pdfDate, SYSTEMTIME* timeOut) {
    if (!pdfDate) {
        return false;
    }
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    Str slice = pdfDate;
    // "D:" at the beginning is optional
    if (str::StartsWith(slice, "D:")) {
        slice = Str(slice.s + 2, slice.len - 2);
    }
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
            prop = m_pdfEngine->GetPropertyTemp(kPropAuthor);
            if (len(prop) > 0) {
                ws = ToWStr(prop);
                chunkValue.SetTextValue(PKEY_Author, ws.s);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Title:
            m_state = PdfFilterState::Date;
            prop = m_pdfEngine->GetPropertyTemp(kPropTitle);
            if (!prop) {
                prop = m_pdfEngine->GetPropertyTemp(kPropSubject);
            }
            if (len(prop) > 0) {
                ws = ToWStr(prop);
                chunkValue.SetTextValue(PKEY_Title, ws.s);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Date:
            m_state = PdfFilterState::Content;
            prop = m_pdfEngine->GetPropertyTemp(kPropModificationDate);
            if (!prop) {
                prop = m_pdfEngine->GetPropertyTemp(kPropCreationDate);
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
                if (!pageText.text) {
                    FreePageText(&pageText);
                    continue;
                }
                TempStr crlfText = str::ReplaceTemp(pageText.text, StrL("\n"), StrL("\r\n"));
                TempWStr text = ToWStrTemp(crlfText);
                chunkValue.SetTextValue(PKEY_Search_Contents, text.s, CHUNK_TEXT);
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
