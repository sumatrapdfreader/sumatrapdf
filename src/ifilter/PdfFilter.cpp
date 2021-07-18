/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EnginePdf.h"

#include "FilterBase.h"
#include "PdfFilterClsid.h"
#include "PdfFilter.h"

void _submitDebugReportIfFunc(__unused bool cond, __unused const char* condStr) {
    // no-op implementation to satisfy SubmitBugReport()
}

VOID PdfFilter::CleanUp() {
    logf("PdfFilter::Cleanup()\n");
    if (m_pdfEngine) {
        delete m_pdfEngine;
        m_pdfEngine = nullptr;
    }
    m_state = PdfFilterState::End;
}

HRESULT PdfFilter::OnInit() {
    logf("PdfFilter::OnInit()\n");
    CleanUp();

    // TODO: EnginePdf::CreateFromStream never returns with
    //       m_pStream instead of a clone - why?

    // load content of PDF document into a seekable stream
    HRESULT res;
    AutoFree data = GetDataFromStream(m_pStream, &res);
    if (data.empty()) {
        return res;
    }

    auto strm = CreateStreamFromData(data.AsSpan());
    ScopedComPtr<IStream> stream(strm);
    if (!stream) {
        return E_FAIL;
    }

    m_pdfEngine = CreateEnginePdfFromStream(stream);
    if (!m_pdfEngine) {
        return E_FAIL;
    }

    m_state = PdfFilterState::Start;
    m_iPageNo = 0;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool PdfDateParse(const WCHAR* pdfDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, L"D:")) {
        pdfDate += 2;
    }
    return str::Parse(pdfDate,
                      L"%4d%2d%2d"
                      L"%2d%2d%2d",
                      &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                      &timeOut->wSecond) != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

HRESULT PdfFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    logf("PdfFilter::GetNextChunkValue()\n");
    AutoFreeWstr str;

    switch (m_state) {
        case PdfFilterState::Start:
            m_state = PdfFilterState::Author;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case PdfFilterState::Author:
            m_state = PdfFilterState::Title;
            str.Set(m_pdfEngine->GetProperty(DocumentProperty::Author));
            if (!str::IsEmpty(str.Get())) {
                chunkValue.SetTextValue(PKEY_Author, str);
                return S_OK;
            }
            // fall through

        case PdfFilterState::Title:
            m_state = PdfFilterState::Date;
            str.Set(m_pdfEngine->GetProperty(DocumentProperty::Title));
            if (!str) {
                str.Set(m_pdfEngine->GetProperty(DocumentProperty::Subject));
            }
            if (!str::IsEmpty(str.Get())) {
                chunkValue.SetTextValue(PKEY_Title, str);
                return S_OK;
            }
            // fall through

        case PdfFilterState::Date:
            m_state = PdfFilterState::Content;
            str.Set(m_pdfEngine->GetProperty(DocumentProperty::ModificationDate));
            if (!str) {
                str.Set(m_pdfEngine->GetProperty(DocumentProperty::CreationDate));
            }
            if (!str::IsEmpty(str.Get())) {
                SYSTEMTIME systime;
                FILETIME filetime;
                if (PdfDateParse(str, &systime) && SystemTimeToFileTime(&systime, &filetime)) {
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
            }
            // fall through

        case PdfFilterState::Content:
            while (++m_iPageNo <= m_pdfEngine->PageCount()) {
                PageText pageText = m_pdfEngine->ExtractPageText(m_iPageNo);
                if (str::IsEmpty(pageText.text)) {
                    FreePageText(&pageText);
                    continue;
                }
                str.Set(pageText.text);
                AutoFreeWstr str2 = str::Replace(str.Get(), L"\n", L"\r\n");
                chunkValue.SetTextValue(PKEY_Search_Contents, str2.Get(), CHUNK_TEXT);
                FreePageText(&pageText);
                return S_OK;
            }
            m_state = PdfFilterState::End;
            // fall through

        case PdfFilterState::End:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
