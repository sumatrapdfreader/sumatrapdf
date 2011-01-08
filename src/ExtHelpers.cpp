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
 * -> IFilter DLL opens an named shared memory map and fills it with the PDF
 *    document content. The name of the file mapping is then passed to a newly
 *    started (windowless) SumatraPDF through the command line.
 *    The DLL also creates two named events for of which the DLL will set
 *    the "-Produce" event when it wants more data and SumatraPDF is supposed to
 *    set the "-Consume" event when the DLL can get the next chunk.
 * <- SumatraPDF reads the whole file content into its own buffer and then
 *    repeatedly waits for the "-Produce" event, puts data for the next chunk
 *    into the memory map and sets the "-Consume" event. When no more data is
 *    available, an empty string is returned.
 * -> IFilter DLL can then repeatedly wait for "-Consume" events, return the next
 *    chunk to the search indexer and reset the "-Produce" event.
 */
DWORD WINAPI UpdateMMapForIndexing(LPVOID IFilterMMap)
{
    PdfEngine engine;
    VStrList pages;
    fz_buffer *filedata = NULL;

    ULONG count;
    if (!IFilterMMap || _stscanf((LPTSTR)IFilterMMap, _T("SumatraPDF-%ul-"), &count) != 1)
        return 1;
    HANDLE hIFilterMMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, (LPTSTR)IFilterMMap);
    if (!hIFilterMMap)
        return 1;

    TCHAR eventName[96];
    tstr_printf_s(eventName, dimof(eventName), _T("%s-Produce"), IFilterMMap);
    HANDLE hProduceEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, eventName);
    tstr_printf_s(eventName, dimof(eventName), _T("%s-Consume"), IFilterMMap);
    HANDLE hConsumeEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, eventName);

    char *data = (char *)MapViewOfFile(hIFilterMMap, FILE_MAP_ALL_ACCESS, 0, 0, count);
    if (!data)
        goto Error;

    filedata = fz_newbuffer(count);
    filedata->len = count;
    memcpy(filedata->data, data, count);
    fz_stream *stm = fz_openbuffer(filedata);
    bool success = engine.load(stm);
    fz_close(stm);
    if (!success)
        goto Error;
    fz_obj *info = engine.getPdfInfo();

    if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
        goto Error;
    str_printf_s(data, count, "Type:document");
    SetEvent(hConsumeEvent);

    if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
        goto Error;
    char *author = pdf_toutf8(fz_dictgets(info, "Author"));
    str_printf_s(data, count, "Author:%s", author);
    free(author);
    SetEvent(hConsumeEvent);

    if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
        goto Error;
    char *title = pdf_toutf8(fz_dictgets(info, "Title"));
    if (str_empty(title)) {
        free(title);
        title = pdf_toutf8(fz_dictgets(info, "Subject"));
    }
    str_printf_s(data, count, "Title:%s", title);
    free(title);
    SetEvent(hConsumeEvent);

    if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
        goto Error;
    char *date = pdf_toutf8(fz_dictgets(info, "ModDate"));
    if (str_empty(date)) {
        free(date);
        date = pdf_toutf8(fz_dictgets(info, "CreationDate"));
    }
    str_printf_s(data, count, "Date:%s", date);
    free(date);
    SetEvent(hConsumeEvent);

    for (int pageNo = 1; pageNo <= engine.pageCount(); pageNo++) {
        if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
            goto Error;
        TCHAR *content = engine.ExtractPageText(pageNo);
        char *contentUTF8 = tstr_to_utf8(content);
        str_printf_s(data, count, "Content:%s", contentUTF8);
        free(contentUTF8);
        free(content);
        SetEvent(hConsumeEvent);
    }

    if (WaitForSingleObject(hProduceEvent, 1000) != WAIT_OBJECT_0)
        goto Error;
    *data = '\0';
    SetEvent(hConsumeEvent);

    CloseHandle(hProduceEvent);
    CloseHandle(hConsumeEvent);
    UnmapViewOfFile(data);
    CloseHandle(hIFilterMMap);
    fz_dropbuffer(filedata);

    return 0;

Error:
    if (hProduceEvent)
        CloseHandle(hProduceEvent);
    if (hConsumeEvent) {
        if (data) {
            *data = '\0';
            SetEvent(hConsumeEvent);
        }
        CloseHandle(hConsumeEvent);
    }
    if (data)
        UnmapViewOfFile(data);
    CloseHandle(hIFilterMMap);
    if (filedata)
        fz_dropbuffer(filedata);
    return 1;
}
