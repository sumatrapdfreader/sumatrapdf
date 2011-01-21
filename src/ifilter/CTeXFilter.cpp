#include "base_util.h"
#include "tstr_util.h"
#include "CTeXFilter.h"

HRESULT CTeXFilter::OnInit()
{
    if (!m_pData) {
        // load content of LaTeX file into m_pData
        STATSTG stat;
        HRESULT res = m_pStream->Stat(&stat, STATFLAG_NONAME);
        if (FAILED(res))
            return res;

        m_pData = (char *)malloc(stat.cbSize.LowPart + 1);
        m_pBuffer = (char *)malloc(stat.cbSize.LowPart + 1);
        if (!m_pData || !m_pBuffer) {
            CleanUp();
            return E_OUTOFMEMORY;
        }

        ULONG read;
        LARGE_INTEGER zero = { 0 };
        m_pStream->Seek(zero, STREAM_SEEK_SET, NULL);
        res = m_pStream->Read(m_pData, stat.cbSize.LowPart, &read);
        if (FAILED(res)) {
            CleanUp();
            return res;
        }

        m_pData[min(stat.cbSize.LowPart, read)] = '\0';
    }

    m_state = STATE_TEX_START;
    m_pPtr = m_pData;
    m_iDepth = 0;

    return S_OK;
}

#define iscmdchar(c) (isalnum(c) || (c) == '_')
#define skipspace(pc) for (; isspace(*(pc)) && *(pc) != '\n'; (pc)++)

// appends a new line, if the last character isn't one already
static inline void addsingleNL(char *base, char **cur)
{
    if (*cur > base && *(*cur - 1) != '\n')
        *(*cur)++ = '\n';
}

// appends a space, if the last character isn't one already
static inline void addsinglespace(char *base, char **cur)
{
    if (*cur > base && !isspace(*(*cur - 1)))
        *(*cur)++ = ' ';
}

// extracts a text block contained within a pair of braces
// (may contain nested braces)
char *CTeXFilter::ExtractBracedBlock()
{
    m_iDepth++;

    char *result = m_pBuffer + (m_pPtr - m_pData);
    char *rptr = result;

    int currDepth = m_iDepth;

    while (*m_pPtr && m_iDepth >= currDepth) {
        switch (*m_pPtr++) {
        case '\\':
            // skip all LaTeX/TeX commands
            if (iscmdchar(*m_pPtr)) {
                // ignore the content of \begin{...} and \end{...}
                if (str_startswith(m_pPtr, "begin{") || str_startswith(m_pPtr, "end{")) {
                    m_pPtr = strchr(m_pPtr, '{') + 1;
                    ExtractBracedBlock();
                    addsingleNL(result, &rptr);
                    break;
                }
                // convert \item to a single dash
                if (str_startswith(m_pPtr, "item") && !iscmdchar(*(m_pPtr + 4))) {
                    m_pPtr += 4;
                    addsingleNL(result, &rptr);
                    *rptr++ = '-';
                    addsinglespace(result, &rptr);
                }
                for (; iscmdchar(*m_pPtr); m_pPtr++);
                skipspace(m_pPtr);
                // ignore command parameters in brackets
                if (*m_pPtr == '[' && strchr(m_pPtr, ']')) {
                    m_pPtr = strchr(m_pPtr, ']') + 1;
                }
                break;
            }
            // handle newlines newlines, spaces, etc.
            if (*m_pPtr == '\\') { addsingleNL(result, &rptr); m_pPtr++; break; }
            if (*m_pPtr == ',')  { addsinglespace(result, &rptr); m_pPtr++; break; }
            if (*m_pPtr == '>')  { *rptr++ = '\t'; m_pPtr++; break; }
            // TODO: handle more international characters
            if (str_startswith(m_pPtr, "'e")) { *rptr++ = 'é'; m_pPtr += 2; break; }
            if (str_startswith(m_pPtr, "`e")) { *rptr++ = 'è'; m_pPtr += 2; break; }
            if (str_startswith(m_pPtr, "`a")) { *rptr++ = 'à'; m_pPtr += 2; break; }
            if (*m_pPtr != '"')
                break;
            m_pPtr++;
            /* fall through */
        case '"':
            // TODO: handle more international characters
            switch (*m_pPtr++) {
            case 'a': *rptr++ = 'ä'; break;
            case 'A': *rptr++ = 'Ä'; break;
            case 'o': *rptr++ = 'ö'; break;
            case 'O': *rptr++ = 'Ö'; break;
            case 'u': *rptr++ = 'ü'; break;
            case 'U': *rptr++ = 'Ü'; break;
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
            if (strchr(m_pPtr, ']') && strchr(m_pPtr, ']') < strchr(m_pPtr, '\n'))
                m_pPtr = strchr(m_pPtr, ']') + 1;
            break;
        case '%':
            // ignore comments until the end of line
            m_pPtr = strchr(m_pPtr, '\n') ? strchr(m_pPtr, '\n') + 1 : m_pPtr + strlen(m_pPtr);
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
            if (isspace(*m_pPtr)) {
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
    char *start, *end;

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
        start = end = NULL;
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
            }
        }
        if (!start)
            goto ContinueParsing;
        skipspace(m_pPtr);
        if (*m_pPtr != '{')
            goto ContinueParsing;
        m_pPtr++;

        if (!strncmp(start, "author", end - start) || !strncmp(start, "title", end - start)) {
            WCHAR *value = multibyte_to_wstr(ExtractBracedBlock(), CP_ACP);
            chunkValue.SetTextValue(*start == 'a' ? PKEY_Author : PKEY_Title, value);
            free(value);
            return S_OK;
        }

        if (!strncmp(start, "begin", end - start) && str_eq(ExtractBracedBlock(), "document"))
            m_state = STATE_TEX_CONTENT;
        goto ContinueParsing;
    case STATE_TEX_CONTENT:
        {
            WCHAR *content = multibyte_to_wstr(ExtractBracedBlock(), CP_ACP);
            chunkValue.SetTextValue(PKEY_Search_Contents, content, CHUNK_TEXT);
            free(content);
            return S_OK;
        }
    default:
        return FILTER_E_END_OF_CHUNKS;
    }
}
