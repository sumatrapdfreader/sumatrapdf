/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"

#include "utils/Log.h"

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

    m_pdfEngine = CreateEngineMupdfFromStream(stream, "foo.pdf");
    if (!m_pdfEngine) {
        return E_FAIL;
    }

    m_state = PdfFilterState::Start;
    m_iPageNo = 0;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool PdfDateParse(const char* pdfDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, "D:")) {
        pdfDate += 2;
    }
    return str::Parse(pdfDate,
                      "%4d%2d%2d"
                      "%2d%2d%2d",
                      &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                      &timeOut->wSecond) != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

// Start, Author, Title, Date, Content, End

static const char* PdfFilterStateToStr(PdfFilterState state) {
    const char* res = seqstrings::IdxToStr(kPdfFilterStateStrs, (int)state);
    return res ? res : "uknown";
}

HRESULT PdfFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    const char* stateStr = PdfFilterStateToStr(m_state);
    logf("PdfFilter::GetNextChunkValue(), state: %s (%d)\n", stateStr, (int)m_state);
    char* prop = nullptr;
    WCHAR* ws = nullptr;
    switch (m_state) {
        case PdfFilterState::Start:
            m_state = PdfFilterState::Author;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case PdfFilterState::Author:
            m_state = PdfFilterState::Title;
            prop = m_pdfEngine->GetPropertyTemp(kPropAuthor);
            if (!str::IsEmpty(prop)) {
                ws = ToWStr(prop);
                chunkValue.SetTextValue(PKEY_Author, ws);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Title:
            m_state = PdfFilterState::Date;
            prop = m_pdfEngine->GetPropertyTemp(kPropTitle);
            if (!prop) {
                prop = m_pdfEngine->GetPropertyTemp(kPropSubject);
            }
            if (!str::IsEmpty(prop)) {
                ws = ToWStr(prop);
                chunkValue.SetTextValue(PKEY_Title, ws);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Date:
            m_state = PdfFilterState::Content;
            prop = m_pdfEngine->GetPropertyTemp(kPropModificationDate);
            if (!prop) {
                prop = m_pdfEngine->GetPropertyTemp(kPropCreationDate);
            }
            if (!str::IsEmpty(prop)) {
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
                if (str::IsEmpty(pageText.text)) {
                    FreePageText(&pageText);
                    continue;
                }
                ws = pageText.text;
                WCHAR* str = str::Replace(ws, L"\n", L"\r\n");
                chunkValue.SetTextValue(PKEY_Search_Contents, str, CHUNK_TEXT);
                str::FreePtr(&str);
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
