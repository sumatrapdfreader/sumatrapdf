/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "FilterBase.h"
#include "PdfFilter.h"
#include "CTeXFilter.h"

HRESULT CTeXFilter::OnInit()
{
    if (!m_pData) {
        // load content of LaTeX file into m_pData
        HRESULT res;
        AutoFree data = GetDataFromStream(m_pStream, &res);
        if (data.empty()) {
            return res;
        }

        m_pData = strconv::ToWideChar(data.data, CP_ACP);
        m_pBuffer = AllocArray<WCHAR>(data.size() + 1);

        if (!m_pData || !m_pBuffer) {
            CleanUp();
            return E_OUTOFMEMORY;
        }
    }

    m_state = STATE_TEX_START;
    m_pPtr = m_pData;
    m_iDepth = 0;

    return S_OK;
}

#define iscmdchar(c) (iswalnum(c) || (c) == '_')
#define skipspace(pc) for (; str::IsWs(*(pc)) && *(pc) != '\n'; (pc)++)
#define skipcomment(pc) while (*(pc) && *(pc)++ != '\n')

// appends a new line, if the last character isn't one already
static inline void addsingleNL(WCHAR *base, WCHAR **cur)
{
    if (*cur > base && *(*cur - 1) != '\n')
        *(*cur)++ = '\n';
}

// appends a space, if the last character isn't one already
static inline void addsinglespace(WCHAR *base, WCHAR **cur)
{
    if (*cur > base && !str::IsWs(*(*cur - 1)))
        *(*cur)++ = ' ';
}

// extracts a text block contained within a pair of braces
// (may contain nested braces)
WCHAR *CTeXFilter::ExtractBracedBlock()
{
    m_iDepth++;

    WCHAR *result = m_pBuffer + (m_pPtr - m_pData);
    WCHAR *rptr = result;

    int currDepth = m_iDepth;

    while (*m_pPtr && m_iDepth >= currDepth) {
        switch (*m_pPtr++) {
        case '\\':
            // skip all LaTeX/TeX commands
            if (iscmdchar(*m_pPtr)) {
                // ignore the content of \begin{...} and \end{...}
                if (str::StartsWith(m_pPtr, L"begin{") || str::StartsWith(m_pPtr, L"end{")) {
                    m_pPtr = wcschr(m_pPtr, '{') + 1;
                    ExtractBracedBlock();
                    addsingleNL(result, &rptr);
                    break;
                }
                // convert \item to a single dash
                if (str::StartsWith(m_pPtr, L"item") && !iscmdchar(*(m_pPtr + 4))) {
                    m_pPtr += 4;
                    addsingleNL(result, &rptr);
                    *rptr++ = '-';
                    addsinglespace(result, &rptr);
                }
                for (; iscmdchar(*m_pPtr); m_pPtr++);
                skipspace(m_pPtr);
                // ignore command parameters in brackets
                if (*m_pPtr == '[' && wcschr(m_pPtr, ']')) {
                    m_pPtr = wcschr(m_pPtr, ']') + 1;
                }
                break;
            }
            // handle newlines newlines, spaces, etc.
            if (*m_pPtr == '\\') { addsingleNL(result, &rptr); m_pPtr++; break; }
            if (*m_pPtr == ',')  { addsinglespace(result, &rptr); m_pPtr++; break; }
            if (*m_pPtr == '>')  { *rptr++ = '\t'; m_pPtr++; break; }
            if (*m_pPtr == '%')  { *rptr++ = '%'; m_pPtr++; break; }
            // TODO: handle more international characters
            if (str::StartsWith(m_pPtr, L"'e")) { *rptr++ = L'é'; m_pPtr += 2; break; }
            if (str::StartsWith(m_pPtr, L"`e")) { *rptr++ = L'è'; m_pPtr += 2; break; }
            if (str::StartsWith(m_pPtr, L"`a")) { *rptr++ = L'à'; m_pPtr += 2; break; }
            if (*m_pPtr != '"')
                break;
            m_pPtr++;
            /* fall through */
        case '"':
            // TODO: handle more international characters
            switch (*m_pPtr++) {
            case 'a': *rptr++ = L'ä'; break;
            case 'A': *rptr++ = L'Ä'; break;
            case 'o': *rptr++ = L'ö'; break;
            case 'O': *rptr++ = L'Ö'; break;
            case 'u': *rptr++ = L'ü'; break;
            case 'U': *rptr++ = L'Ü'; break;
            case '`': case '\'': *rptr++ = '"'; break;
            default: *rptr++ = *(m_pPtr - 1); break;
            }
            break;
        case '{':
            m_iDepth++;
            break;
        case '}':
            m_iDepth--;
            if (*m_pPtr == '{')
                addsinglespace(result, &rptr);
            break;
        case '[':
            // ignore command parameters in brackets
            if (wcschr(m_pPtr, ']') && wcschr(m_pPtr, ']') < wcschr(m_pPtr, '\n'))
                m_pPtr = wcschr(m_pPtr, ']') + 1;
            break;
        case '%':
            skipcomment(m_pPtr);
            break;
        case '&':
            *rptr++ = '\t';
            break;
        case '~':
            addsinglespace(result, &rptr);
            break;
        case '\n':
            // treat single newlines as spaces
            if (*m_pPtr == '\n' || *m_pPtr == '\r') {
                addsingleNL(result, &rptr);
                m_pPtr++;
                break;
            }
        default:
            m_pPtr--;
            if (str::IsWs(*m_pPtr)) {
                addsinglespace(result, &rptr);
                m_pPtr++;
                break;
            }
            *rptr++ = *m_pPtr++;
            break;
        }
    }

    if (*m_pPtr == '}')
        m_pPtr++;
    *rptr = '\0';
    return result;
}

HRESULT CTeXFilter::GetNextChunkValue(CChunkValue &chunkValue)
{
    WCHAR *start, *end;

ContinueParsing:
    if (!*m_pPtr && m_state == STATE_TEX_PREAMBLE) {
        // if there was no preamble, treat the whole document as content
        m_pPtr = m_pData;
        m_iDepth = 0;
        m_state = STATE_TEX_CONTENT;
    }
    else if (!*m_pPtr) {
        m_state = STATE_TEX_END;
    }

    switch (m_state)
    {
    case STATE_TEX_START:
        m_state = STATE_TEX_PREAMBLE;
        chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        return S_OK;
    case STATE_TEX_PREAMBLE:
        // the preamble (i.e. everything before \begin{document}) may contain
        // \author{...} and \title{...} commands
        start = end = nullptr;
        while (*m_pPtr && !start) {
            switch (*m_pPtr++){
            case '\\':
                if (iscmdchar(*m_pPtr)) {
                    start = m_pPtr;
                    for (end = start; iscmdchar(*m_pPtr); m_pPtr++, end++);
                    break;
                }
                if (*m_pPtr)
                    m_pPtr++;
                break;
            case '{':
                ExtractBracedBlock();
                break;
            case '%':
                skipcomment(m_pPtr);
                break;
            }
        }
        if (!start)
            goto ContinueParsing;
        skipspace(m_pPtr);
        if (*m_pPtr != '{')
            goto ContinueParsing;
        m_pPtr++;

        if (!wcsncmp(start, L"author", end - start) || !wcsncmp(start, L"title", end - start)) {
            chunkValue.SetTextValue(*start == 'a' ? PKEY_Author : PKEY_Title, ExtractBracedBlock());
            return S_OK;
        }

        if (!wcsncmp(start, L"begin", end - start) && str::Eq(ExtractBracedBlock(), L"document"))
            m_state = STATE_TEX_CONTENT;
        goto ContinueParsing;
    case STATE_TEX_CONTENT:
        chunkValue.SetTextValue(PKEY_Search_Contents, ExtractBracedBlock(), CHUNK_TEXT);
        return S_OK;
    default:
        return FILTER_E_END_OF_CHUNKS;
    }
}
