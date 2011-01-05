#include "base_util.h"
#include "PdfEngine.h"
#include "WindowInfo.h"
#include "ExtHelpers.h"
#include "WinUtil.hpp"
#include "vstrlist.h"

bool gPluginMode = false;

void MakePluginWindow(WindowInfo *win, HWND hwndParent)
{
    assert(IsWindow(hwndParent));
    assert(gPluginMode);
    win->pluginParent = hwndParent;

    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP|WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);

    RECT rc;
    SetParent(win->hwndFrame, hwndParent);
    GetClientRect(hwndParent, &rc);
    MoveWindow(win->hwndFrame, 0, 0, RectDx(&rc), RectDy(&rc), FALSE);
    // from here on, we depend on the plugin's host to resize us
    SetFocus(win->hwndFrame);
}

/**
 * Communication protocol:
 * -> IFilter DLL opens an anonymous shared memory map and fills it with
 *    "IFilterMMap 1.3 %ul\0%s" where %ul is the size of the PDF document content
 *    included as %s. Finally, the handle to the mapping object is past to a
 *    newly started (windowless) SumatraPDF.
 * <- SumatraPDF replaces the memory map's content with a double-zero terminated
 *    list of zero terminated properties extracted from the PDF document, currently
 *    "Author:", "Title:", "Date:" and "Content:", with the first entry being a
 *    "IFilterMMap 1.3" header. Once done, SumatraPDF exits immediately.
 * -> IFilter DLL can then easily iterate over these properties and return them
 *    through the IFilter API.
 */

#define MMAP_HEADER_SIZE 64

void UpdateMMapForIndexing(HANDLE hIFilterMMap)
{
    PdfEngine engine;
    VStrList pages;
    fz_buffer *filedata = NULL;

    char *data = (char *)MapViewOfFile(hIFilterMMap, FILE_MAP_ALL_ACCESS, 0, 0, MMAP_HEADER_SIZE);
    assert(data);
    if (!data)
        return;
    ULONG count;
    if (sscanf(data, "IFilterMMap 1.3 %ul", &count) != 1)
        goto Error;
    UnmapViewOfFile(data);

    data = (char *)MapViewOfFile(hIFilterMMap, FILE_MAP_ALL_ACCESS, 0, 0, count + MMAP_HEADER_SIZE);
    if (!data)
        goto Error;

    filedata = fz_newbuffer(count);
    filedata->len = count;
    memcpy(filedata->data, data + str_len(data) + 1, count);
    fz_stream *stm = fz_openbuffer(filedata);
    bool success = engine.load(stm);
    fz_close(stm);
    if (!success)
        goto Error;

    char *out = data, *end = data + count;
    out += str_printf_s(out, end - out, "IFilterMMap 1.3") + 1;
    fz_obj *info = engine.getPdfInfo();
    char *author = pdf_toutf8(fz_dictgets(info, "Author"));
    out += str_printf_s(out, end - out, "Author:%s", author) + 1;
    free(author);
    char *title = pdf_toutf8(fz_dictgets(info, "Title"));
    out += str_printf_s(out, end - out, "Title:%s", title) + 1;
    free(title);
    char *date = pdf_toutf8(fz_dictgets(info, "ModDate"));
    if (str_empty(date)) {
        free(date);
        date = pdf_toutf8(fz_dictgets(info, "CreationDate"));
    }
    out += str_printf_s(out, end - out, "Date:%s", date) + 1;
    free(date);

    for (int pageNo = 1; pageNo <= engine.pageCount(); pageNo++)
        pages.push_back(engine.ExtractPageText(pageNo, _T(DOS_NEWLINE)));
    TCHAR *content = pages.join();
    char *contentUTF8 = tstr_to_utf8(content);
    int len = str_printf_s(out, end - out, "Content:%s", contentUTF8);
    out += (len > 0 ? len : strlen(out)) + 1;
    free(contentUTF8);
    free(content);

    // ensure the double-zero termination
    if (out < end)
        *out = '\0';
    *(end - 1) = *(end - 2) = '\0';

    UnmapViewOfFile(data);
    fz_dropbuffer(filedata);
    return;

Error:
    if (data) {
        *data = 0;
        UnmapViewOfFile(data);
    }
    if (filedata)
        fz_dropbuffer(filedata);
}
