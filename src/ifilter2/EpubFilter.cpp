/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"
#include "EpubFilter.h"

#include "utils/Log.h"

VOID EpubFilter::CleanUp() {
    log("EpubFilter::Cleanup()\n");
    if (m_extractedData) {
        FreeExtractedData(m_extractedData);
        m_extractedData = nullptr;
    }
    m_state = STATE_EPUB_END;
}

HRESULT EpubFilter::OnInit() {
    log("EpubFilter::OnInit()\n");

    CleanUp();

    // Load content of EPUB document into a seekable stream
    HRESULT res;
    ByteSlice data = GetDataFromStream(m_pStream, &res);
    if (data.empty()) {
        return res;
    }

    // Extract data via pipe communication with SumatraPDF.exe
    m_extractedData = ExtractDataViaPipe(IFilterFileType::EPUB, data);
    data.Free();

    if (!m_extractedData) {
        return E_FAIL;
    }

    m_state = STATE_EPUB_START;
    m_iPageNo = 0;
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

HRESULT EpubFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    log("EpubFilter::GetNextChunkValue()\n");

    if (!m_extractedData) {
        return FILTER_E_END_OF_CHUNKS;
    }

    WCHAR* ws = nullptr;

    switch (m_state) {
        case STATE_EPUB_START:
            m_state = STATE_EPUB_AUTHOR;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case STATE_EPUB_AUTHOR:
            m_state = STATE_EPUB_TITLE;
            if (!str::IsEmpty(m_extractedData->author)) {
                ws = ToWStrTemp(m_extractedData->author);
                chunkValue.SetTextValue(PKEY_Author, ws);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_TITLE:
            m_state = STATE_EPUB_DATE;
            if (!str::IsEmpty(m_extractedData->title)) {
                ws = ToWStrTemp(m_extractedData->title);
                chunkValue.SetTextValue(PKEY_Title, ws);
                return S_OK;
            }
            // fall through

        case STATE_EPUB_DATE:
            m_state = STATE_EPUB_CONTENT;
            if (!str::IsEmpty(m_extractedData->date)) {
                SYSTEMTIME systime;
                if (IsoDateParse(m_extractedData->date, &systime)) {
                    FILETIME filetime;
                    SystemTimeToFileTime(&systime, &filetime);
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
            }
            // fall through

        case STATE_EPUB_CONTENT:
            // Return page texts one by one
            while (++m_iPageNo <= m_extractedData->pageCount) {
                WCHAR* pageText = m_extractedData->pageTexts ? m_extractedData->pageTexts[m_iPageNo - 1] : nullptr;
                if (str::IsEmpty(pageText)) {
                    continue;
                }
                chunkValue.SetTextValue(PKEY_Search_Contents, pageText, CHUNK_TEXT);
                return S_OK;
            }
            m_state = STATE_EPUB_END;
            // fall through

        case STATE_EPUB_END:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
