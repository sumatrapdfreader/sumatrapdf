/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct WindowTab;

// How a SelectionHandlers entry sends the selection (the Method setting).
// Anything unrecognized, including it being unset, means Get, so settings files
// written before Method existed keep working unchanged.
enum class SelectionSendMethod {
    Get,            // substitute into the URL and open it in the browser
    Post,           // http POST from inside SumatraPDF
    PostViaBrowser, // auto-submitting html form opened in the browser
};

SelectionSendMethod ParseSelectionSendMethod(Str s);
TempStr ExpandSelectionVarsTemp(Str pattern, Str selection, bool urlEncodeSelection, int budget = 0,
                                bool* didTruncateOut = nullptr);
void SelectionHandlerPost(WindowTab* tab, Str url, Str bodyPattern, Str contentType, Str headers, Str selection);
void SelectionHandlerPostViaBrowser(WindowTab* tab, Str url, Str bodyPattern, Str selection);
