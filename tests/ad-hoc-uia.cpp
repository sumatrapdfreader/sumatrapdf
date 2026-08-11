// UI Automation *client* used by tests/ad-hoc-uia.ts to check what a screen
// reader (Narrator, NVDA, ...) can see and read inside SumatraPDF's canvas.
// It is compiled on demand by that test, not by the normal build:
//   cl /nologo /EHsc /std:c++17 ad-hoc-uia.cpp /Fe<out.exe>
//      /link ole32.lib oleaut32.lib user32.lib uiautomationcore.lib
//
// Usage: ad-hoc-uia.exe <canvas hwnd as decimal>
// Prints `key=value` lines; values escape \ | and newlines.
#include <windows.h>
#include <uiautomation.h>
#include <stdio.h>

static void PrintEscaped(const char* key, BSTR s) {
    printf("%s=", key);
    for (int i = 0; s && s[i]; i++) {
        wchar_t c = s[i];
        if (c == L'\r') {
            continue;
        }
        if (c == L'\n') {
            printf("\\n");
        } else if (c == L'\\') {
            printf("\\\\");
        } else if (c == L'|') {
            printf("\\p");
        } else {
            printf("%lc", c);
        }
    }
    printf("\n");
}

// text of the range, or an empty string
static BSTR RangeText(IUIAutomationTextRange* r, int maxLen) {
    BSTR s = nullptr;
    if (r) {
        r->GetText(maxLen, &s);
    }
    return s;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("error=usage: ad-hoc-uia.exe <canvas hwnd>\n");
        return 1;
    }
    HWND canvas = (HWND)(intptr_t)_atoi64(argv[1]);
    SetConsoleOutputCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IUIAutomation* uia = nullptr;
    HRESULT hr =
        CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&uia);
    if (FAILED(hr) || !uia) {
        printf("error=CoCreateInstance(CUIAutomation) failed 0x%08x\n", (unsigned)hr);
        return 1;
    }

    IUIAutomationElement* canvasElem = nullptr;
    hr = uia->ElementFromHandle(canvas, &canvasElem);
    if (FAILED(hr) || !canvasElem) {
        printf("error=ElementFromHandle failed 0x%08x\n", (unsigned)hr);
        return 1;
    }
    BOOL kbFocusable = FALSE;
    canvasElem->get_CurrentIsKeyboardFocusable(&kbFocusable);
    printf("canvas.found=1\n");
    printf("canvas.kbFocusable=%d\n", (int)kbFocusable);

    // the document element is the canvas child whose control type is Document
    IUIAutomationTreeWalker* raw = nullptr;
    uia->get_RawViewWalker(&raw);
    IUIAutomationElement* doc = nullptr;
    if (raw) {
        IUIAutomationElement* it = nullptr;
        raw->GetFirstChildElement(canvasElem, &it);
        while (it) {
            CONTROLTYPEID ct = 0;
            it->get_CurrentControlType(&ct);
            if (ct == UIA_DocumentControlTypeId) {
                doc = it;
                break;
            }
            IUIAutomationElement* next = nullptr;
            raw->GetNextSiblingElement(it, &next);
            it->Release();
            it = next;
        }
    }
    if (!doc) {
        printf("doc.found=0\n");
        return 0;
    }
    printf("doc.found=1\n");

    BOOL isCtrl = FALSE, isContent = FALSE;
    doc->get_CurrentIsControlElement(&isCtrl);
    doc->get_CurrentIsContentElement(&isContent);
    printf("doc.isControlElement=%d\n", (int)isCtrl);
    printf("doc.isContentElement=%d\n", (int)isContent);
    BSTR name = nullptr;
    doc->get_CurrentName(&name);
    PrintEscaped("doc.name", name);
    if (name) {
        SysFreeString(name);
    }

    IUIAutomationTextPattern* tp = nullptr;
    doc->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern), (void**)&tp);
    printf("doc.hasTextPattern=%d\n", tp ? 1 : 0);
    if (!tp) {
        return 0;
    }

    IUIAutomationTextRange* docRange = nullptr;
    tp->get_DocumentRange(&docRange);
    BSTR all = RangeText(docRange, 400);
    PrintEscaped("doc.text", all);
    if (all) {
        SysFreeString(all);
    }

    // walking by character / word / line is how a screen reader reads: each
    // step must land on new text
    struct {
        TextUnit unit;
        const char* key;
        int steps;
        int maxLen;
    } walks[] = {{TextUnit_Character, "walk.char", 4, 4},
                 {TextUnit_Word, "walk.word", 3, 40},
                 {TextUnit_Line, "walk.line", 3, 80},
                 {TextUnit_Page, "walk.page", 2, 40}};
    for (auto& w : walks) {
        IUIAutomationTextRange* r = nullptr;
        tp->get_DocumentRange(&r);
        if (!r) {
            continue;
        }
        r->MoveEndpointByRange(TextPatternRangeEndpoint_End, r, TextPatternRangeEndpoint_Start);
        r->ExpandToEnclosingUnit(w.unit);
        BSTR s = RangeText(r, w.maxLen);
        char key0[64];
        sprintf_s(key0, "%s0", w.key);
        PrintEscaped(key0, s); // the unit we start on
        if (s) {
            SysFreeString(s);
        }
        for (int i = 1; i <= w.steps; i++) {
            int moved = 0;
            r->Move(w.unit, 1, &moved);
            BSTR s2 = RangeText(r, w.maxLen);
            char key[64];
            sprintf_s(key, "%s%d", w.key, i);
            PrintEscaped(key, s2);
            if (s2) {
                SysFreeString(s2);
            }
        }
        r->Release();
    }

    // say-all: read the whole document line by line, like a screen reader does
    IUIAutomationTextRange* sayAll = nullptr;
    tp->get_DocumentRange(&sayAll);
    if (sayAll) {
        sayAll->MoveEndpointByRange(TextPatternRangeEndpoint_End, sayAll, TextPatternRangeEndpoint_Start);
        sayAll->ExpandToEnclosingUnit(TextUnit_Line);
        const int kMaxLines = 2000;
        int lines = 0;
        for (; lines < kMaxLines; lines++) {
            int moved = 0;
            if (FAILED(sayAll->Move(TextUnit_Line, 1, &moved)) || moved == 0) {
                break;
            }
        }
        printf("sayall.lines=%d\n", lines);
        printf("sayall.terminated=%d\n", lines < kMaxLines ? 1 : 0);
        sayAll->Release();
    }

    // reading what is under the mouse / finger: take the on-screen rectangle of
    // a word, hand its center to RangeFromPoint and see whether we get that
    // same word back
    {
        IUIAutomationTextRange* word = nullptr;
        tp->get_DocumentRange(&word);
        if (word) {
            word->MoveEndpointByRange(TextPatternRangeEndpoint_End, word, TextPatternRangeEndpoint_Start);
            word->ExpandToEnclosingUnit(TextUnit_Word);
            BSTR wantS = RangeText(word, 40);
            PrintEscaped("point.word", wantS);
            SAFEARRAY* rects = nullptr;
            HRESULT hrR = word->GetBoundingRectangles(&rects);
            double cx = 0, cy = 0;
            bool haveRect = false;
            if (SUCCEEDED(hrR) && rects) {
                LONG lo = 0, hi = -1;
                SafeArrayGetLBound(rects, 1, &lo);
                SafeArrayGetUBound(rects, 1, &hi);
                if (hi - lo + 1 >= 4) {
                    double v[4]{};
                    for (LONG i = 0; i < 4; i++) {
                        LONG idx = lo + i;
                        SafeArrayGetElement(rects, &idx, &v[i]);
                    }
                    cx = v[0] + (v[2] / 2);
                    cy = v[1] + (v[3] / 2);
                    haveRect = v[2] > 0 && v[3] > 0;
                }
                SafeArrayDestroy(rects);
            }
            printf("point.haveRect=%d\n", haveRect ? 1 : 0);
            printf("point.x=%d\npoint.y=%d\n", (int)cx, (int)cy);
            if (haveRect) {
                POINT pt{(LONG)cx, (LONG)cy};
                IUIAutomationTextRange* atPt = nullptr;
                HRESULT hrP = tp->RangeFromPoint(pt, &atPt);
                printf("point.hr=0x%08x\n", (unsigned)hrP);
                if (SUCCEEDED(hrP) && atPt) {
                    atPt->ExpandToEnclosingUnit(TextUnit_Word);
                    BSTR gotS = RangeText(atPt, 40);
                    PrintEscaped("point.wordAtPoint", gotS);
                    if (gotS) {
                        SysFreeString(gotS);
                    }
                    atPt->Release();
                }
            }
            if (wantS) {
                SysFreeString(wantS);
            }
            word->Release();
        }
    }

    if (docRange) {
        docRange->Release();
    }
    tp->Release();
    doc->Release();
    if (raw) {
        raw->Release();
    }
    canvasElem->Release();
    uia->Release();
    CoUninitialize();
    return 0;
}
