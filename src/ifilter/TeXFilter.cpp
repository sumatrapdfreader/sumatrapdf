/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "TeXFilter.h"

HRESULT TeXFilter::OnInit() {
    // m_data changes when Load() is called again
    CleanUp();
    if (str::IsNull(m_data)) {
        return E_FAIL;
    }

    Str raw = m_data;
    bool hasUtf8Bom = str::TrimPrefix(raw, StrL("\xEF\xBB\xBF"));
    const u8* s = (const u8*)raw.s;
    const u8* end = s + len(raw);
    bool isUtf8 = hasUtf8Bom || isLegalUTF8String(&s, end);
    WStr wdata = isUtf8 ? ToWStr(raw) : strconv::StrCPToWStr(raw, CP_ACP);
    m_pData = wdata.s;
    m_pBuffer = AllocArray<WCHAR>(wdata.len + 1);

    if (!m_pData || !m_pBuffer) {
        CleanUp();
        return E_OUTOFMEMORY;
    }

    m_state = STATE_TEX_START;
    m_pPtr = m_pData;
    m_iDepth = 0;

    return S_OK;
}

#define iscmdchar(c) (iswalnum(c) || (c) == '_')
#define skipspace(pc) for (; wstr::IsWs(*(pc)) && *(pc) != '\n'; (pc)++)
#define skipcomment(pc) while (*(pc) && *(pc)++ != '\n')

// appends a new line, if the last character isn't one already
static inline void addsingleNL(WCHAR* base, WCHAR** cur) {
    if (*cur > base && *(*cur - 1) != '\n') {
        *(*cur)++ = '\n';
    }
}

// appends a space, if the last character isn't one already
static inline void addsinglespace(WCHAR* base, WCHAR** cur) {
    if (*cur > base && !wstr::IsWs(*(*cur - 1))) {
        *(*cur)++ = ' ';
    }
}

// extracts a text block contained within a pair of braces
// (may contain nested braces)
WStr TeXFilter::ExtractBracedBlock() {
    m_iDepth++;

    WCHAR* result = m_pBuffer + (m_pPtr - m_pData);
    WCHAR* rptr = result;

    int currDepth = m_iDepth;

    while (*m_pPtr && m_iDepth >= currDepth) {
        switch (*m_pPtr++) {
            case '\\':
                // skip all LaTeX/TeX commands
                if (iscmdchar(*m_pPtr)) {
                    // ignore the content of \begin{...} and \end{...}
                    if (wstr::StartsWith(m_pPtr, WStrL(L"begin{"))) {
                        m_pPtr += 6;
                        int depth = 1;
                        while (*m_pPtr && depth > 0) {
                            if (*m_pPtr == '{') {
                                depth++;
                            } else if (*m_pPtr == '}') {
                                depth--;
                            }
                            m_pPtr++;
                        }
                        addsingleNL(result, &rptr);
                        break;
                    }
                    if (wstr::StartsWith(m_pPtr, WStrL(L"end{"))) {
                        m_pPtr += 4;
                        int depth = 1;
                        while (*m_pPtr && depth > 0) {
                            if (*m_pPtr == '{') {
                                depth++;
                            } else if (*m_pPtr == '}') {
                                depth--;
                            }
                            m_pPtr++;
                        }
                        addsingleNL(result, &rptr);
                        break;
                    }
                    // convert \item to a single dash
                    if (wstr::StartsWith(m_pPtr, WStrL(L"item")) && !iscmdchar(*(m_pPtr + 4))) {
                        m_pPtr += 4;
                        addsingleNL(result, &rptr);
                        *rptr++ = '-';
                        addsinglespace(result, &rptr);
                    }
                    for (; iscmdchar(*m_pPtr); m_pPtr++) {
                        ;
                    }
                    skipspace(m_pPtr);
                    // ignore command parameters in brackets
                    if (*m_pPtr == '[') {
                        WCHAR* endBracket = wcschr(m_pPtr, L']');
                        if (endBracket) {
                            m_pPtr = endBracket + 1;
                        }
                    }
                    break;
                }
                // handle newlines newlines, spaces, etc.
                if (*m_pPtr == '\\') {
                    addsingleNL(result, &rptr);
                    m_pPtr++;
                    break;
                }
                if (*m_pPtr == ',') {
                    addsinglespace(result, &rptr);
                    m_pPtr++;
                    break;
                }
                if (*m_pPtr == '>') {
                    *rptr++ = '\t';
                    m_pPtr++;
                    break;
                }
                if (*m_pPtr == '%') {
                    *rptr++ = '%';
                    m_pPtr++;
                    break;
                }
                // TODO: handle more international characters
                if (wstr::StartsWith(m_pPtr, WStrL(L"'e"))) {
                    *rptr++ = L'é';
                    m_pPtr += 2;
                    break;
                }
                if (wstr::StartsWith(m_pPtr, WStrL(L"`e"))) {
                    *rptr++ = L'è';
                    m_pPtr += 2;
                    break;
                }
                if (wstr::StartsWith(m_pPtr, WStrL(L"`a"))) {
                    *rptr++ = L'à';
                    m_pPtr += 2;
                    break;
                }
                if (*m_pPtr != '"') {
                    break;
                }
                m_pPtr++;
                /* fall through */
            case '"':
                // a trailing " must not step past the terminating NUL
                if (!*m_pPtr) {
                    break;
                }
                // TODO: handle more international characters
                switch (*m_pPtr++) {
                    case 'a':
                        *rptr++ = L'ä';
                        break;
                    case 'A':
                        *rptr++ = L'Ä';
                        break;
                    case 'o':
                        *rptr++ = L'ö';
                        break;
                    case 'O':
                        *rptr++ = L'Ö';
                        break;
                    case 'u':
                        *rptr++ = L'ü';
                        break;
                    case 'U':
                        *rptr++ = L'Ü';
                        break;
                    case '`':
                    case '\'':
                        *rptr++ = '"';
                        break;
                    default:
                        *rptr++ = *(m_pPtr - 1);
                        break;
                }
                break;
            case '{':
                m_iDepth++;
                break;
            case '}':
                m_iDepth--;
                if (*m_pPtr == '{') {
                    addsinglespace(result, &rptr);
                }
                break;
            case '[': {
                // ignore command parameters in brackets on the same line
                WCHAR* p = m_pPtr;
                while (*p && *p != L']' && *p != L'\n') {
                    p++;
                }
                if (*p == L']') {
                    m_pPtr = p + 1;
                }
                break;
            }
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
                [[fallthrough]];
            default:
                m_pPtr--;
                if (wstr::IsWs(*m_pPtr)) {
                    addsinglespace(result, &rptr);
                    m_pPtr++;
                    break;
                }
                *rptr++ = *m_pPtr++;
                break;
        }
    }

    if (*m_pPtr == '}') {
        m_pPtr++;
    }
    *rptr = '\0';
    return WStr(result, (int)(rptr - result));
}

HRESULT TeXFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    if (!m_pPtr) {
        return FILTER_E_END_OF_CHUNKS;
    }
    WCHAR *start, *end;

ContinueParsing:
    if (!*m_pPtr && m_state == STATE_TEX_PREAMBLE) {
        // if there was no preamble, treat the whole document as content
        m_pPtr = m_pData;
        m_iDepth = 0;
        m_state = STATE_TEX_CONTENT;
    } else if (!*m_pPtr) {
        m_state = STATE_TEX_END;
    }

    switch (m_state) {
        case STATE_TEX_START:
            m_state = STATE_TEX_PREAMBLE;
            return chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
        case STATE_TEX_PREAMBLE:
            // the preamble (i.e. everything before \begin{document}) may contain
            // \author{...} and \title{...} commands
            start = end = nullptr;
            while (*m_pPtr && !start) {
                switch (*m_pPtr++) {
                    case '\\':
                        if (iscmdchar(*m_pPtr)) {
                            start = m_pPtr;
                            for (end = start; iscmdchar(*m_pPtr); m_pPtr++, end++) {
                                ;
                            }
                            break;
                        }
                        if (*m_pPtr) {
                            m_pPtr++;
                        }
                        break;
                    case '{':
                        ExtractBracedBlock();
                        break;
                    case '%':
                        skipcomment(m_pPtr);
                        break;
                }
            }
            if (!start) {
                goto ContinueParsing;
            }
            skipspace(m_pPtr);
            if (*m_pPtr != '{') {
                goto ContinueParsing;
            }
            m_pPtr++;

            {
                int cmdLen = (int)(end - start);
                WStr cmd(start, cmdLen);
                if (wstr::Eq(cmd, WStrL(L"author")) || wstr::Eq(cmd, WStrL(L"title"))) {
                    return chunkValue.SetTextValue(*start == 'a' ? PKEY_Author : PKEY_Title, ExtractBracedBlock().s);
                }

                if (wstr::Eq(cmd, WStrL(L"begin")) && wstr::Eq(ExtractBracedBlock(), WStrL(L"document"))) {
                    m_state = STATE_TEX_CONTENT;
                }
            }
            goto ContinueParsing;
        case STATE_TEX_CONTENT:
            return chunkValue.SetTextValue(PKEY_Search_Contents, ExtractBracedBlock().s, CHUNK_TEXT);
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
