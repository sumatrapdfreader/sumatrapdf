/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Sending selected text to a web service (the SelectionHandlers setting).
//
// Three ways to send it, chosen by the Method field:
//
//  GET (default)     the selection is URL-encoded into the URL and the URL is
//                    opened in the browser. Simple, works with any search or
//                    translation site, but a URL can only hold so much text -
//                    see kMaxUrlEncodedLen. This is what every
//                    SelectionHandlers entry did before Method existed.
//
//  POST              we make the http request ourselves and put the selection
//                    in the body, so there is no length limit. Supports custom
//                    headers, which is how api services authenticate. The
//                    browser is not involved, so the service does NOT see the
//                    user's cookies or logins - an api key in Headers is the
//                    only credential it gets. The response is reported in a
//                    notification, not rendered.
//
//  POST-VIA-BROWSER  we write a temp html page containing a form that submits
//                    itself, and open it in the browser. Also unlimited in
//                    length, and because the browser sends it, the service sees
//                    the user's normal session - which is what you want for a
//                    site you're logged into. Custom headers are impossible
//                    this way (a form submission can't set them).
//
// Why WinHTTP rather than the WinINet used elsewhere in base/Http_win.cpp:
// WinINet shares Internet Explorer's cookie jar and cache, so a request would
// carry whatever cookies happen to be lying around to a third-party endpoint.
// For calls that are meant to be authenticated only by an explicit api key,
// that's both surprising and a privacy leak.

#include "base/Base.h"
#include "base/Win.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/JsonParser.h"
#include "base/Http.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Notifications.h"
#include "Translations.h"
#include "SelectionHandlers.h"

constexpr const char* kUserLangStr = "${userlang}";
constexpr const char* kSelectionStr = "${selection}";
constexpr const char* kSelectionJsonStr = "${selectionjson}";
constexpr const char* kSelectionFileStr = "${selectionfile}";

// how much of the response body to show in the notification. The point is to
// tell the user whether it worked and why not, not to render the result
constexpr int kMaxResponseInNotification = 300;

// Parses the Method setting. See SelectionSendMethod in the header.
SelectionSendMethod ParseSelectionSendMethod(Str s) {
    if (str::EqI(s, StrL("POST"))) {
        return SelectionSendMethod::Post;
    }
    if (str::EqI(s, StrL("POST-VIA-BROWSER"))) {
        return SelectionSendMethod::PostViaBrowser;
    }
    // unset, "GET", or a typo: behave the way handlers did before Method existed
    return SelectionSendMethod::Get;
}

// Writes the selection to a temp file and returns its path, so a handler can
// hand arbitrarily long text to an external program without going near a
// command-line length limit. Returns empty on failure.
static TempStr WriteSelectionToTempFileTemp(Str selection, Str ext) {
    TempStr dir = GetTempDirTemp();
    if (str::IsEmptyOrWhiteSpace(dir)) {
        return {};
    }
    // include the pid so two instances don't fight over the same file
    TempStr name = fmt("SumatraPDF-selection-%d%s", (int)GetCurrentProcessId(), ext);
    TempStr path = path::JoinTemp(dir, name);
    if (!file::WriteFile(path, selection)) {
        return {};
    }
    return path;
}

// Expands ${selection}, ${selectionjson}, ${selectionfile} and ${userlang} in
// `pattern`. `urlEncodeSelection` picks how ${selection} is escaped: URL
// encoding for a URL, none for a request body. When URL-encoding, `budget` caps
// the encoded length and *didTruncateOut says whether it didn't all fit.
TempStr ExpandSelectionVarsTemp(Str pattern, Str selection, bool urlEncodeSelection, int budget, bool* didTruncateOut) {
    if (didTruncateOut) {
        *didTruncateOut = false;
    }
    Str lang = trans::GetCurrentLangCode();
    TempStr res = str::ReplaceNoCaseTemp(pattern, kUserLangStr, lang);

    // do the file and json forms first: they must not be affected by whatever
    // escaping ${selection} uses
    if (str::ContainsI(res, kSelectionFileStr)) {
        TempStr path = WriteSelectionToTempFileTemp(selection, StrL(".txt"));
        res = str::ReplaceNoCaseTemp(res, kSelectionFileStr, path);
    }
    if (str::ContainsI(res, kSelectionJsonStr)) {
        res = str::ReplaceNoCaseTemp(res, kSelectionJsonStr, json::EscapeStrTemp(selection));
    }

    if (urlEncodeSelection) {
        int b = budget > 0 ? budget : (kMaxUrlEncodedLen - len(res));
        TempStr enc = URLEncodeMayTruncateTemp(selection, b, didTruncateOut);
        return str::ReplaceNoCaseTemp(res, kSelectionStr, enc);
    }
    return str::ReplaceNoCaseTemp(res, kSelectionStr, selection);
}

static void ShowSelectionHandlerNotification(WindowTab* tab, Str msg, bool isWarning) {
    if (!tab || !tab->win) {
        return;
    }
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.tab = tab;
    args.warning = isWarning;
    args.timeoutMs = isWarning ? 8000 : 4000;
    args.msg = msg;
    // the message can embed the handler's HTTP response, so no tip markup
    args.plainText = true;
    ShowNotification(args);
}

//--- Method = POST (WinHTTP)

struct PostRequest {
    // captured on the ui thread; the worker must not touch tab/win
    Str url;
    Str body;
    Str contentType;
    Str headers;
    WindowTab* tab = nullptr;

    // filled in by the worker
    DWORD statusCode = 0;
    DWORD winErr = 0;
    Str response;
};

static void FreePostRequest(PostRequest* req) {
    str::Free(req->url);
    str::Free(req->body);
    str::Free(req->contentType);
    str::Free(req->headers);
    str::Free(req->response);
    delete req;
}

// Reports the result back on the ui thread. Deliberately terse: this is a
// "did it work" channel, not a viewer.
static void PostRequestFinished(PostRequest* req) {
    if (!IsWindowTabValid(req->tab)) {
        FreePostRequest(req);
        return;
    }
    bool ok = req->statusCode >= 200 && req->statusCode < 300;
    TempStr msg;
    if (req->winErr != 0) {
        msg = fmt(_TRA("Sending selection failed (error %d)").s, (int)req->winErr);
    } else if (ok) {
        msg = fmt(_TRA("Sent selection (HTTP %d)").s, (int)req->statusCode);
    } else {
        msg = fmt(_TRA("Sending selection failed (HTTP %d)").s, (int)req->statusCode);
    }
    if (len(req->response) > 0) {
        TempStr body = req->response;
        if (len(body) > kMaxResponseInNotification) {
            body = str::DupTemp(Str(body.s, kMaxResponseInNotification));
            body = str::JoinTemp(body, StrL("..."));
        }
        msg = str::JoinTemp(msg, StrL("\n"), body);
    }
    ShowSelectionHandlerNotification(req->tab, msg, !ok || req->winErr != 0);
    FreePostRequest(req);
}

static void PostRequestThread(PostRequest* req) {
    HttpRsp rsp;
    HttpPostUrl(req->url, req->contentType, req->headers, req->body, &rsp);
    req->statusCode = rsp.httpStatusCode;
    req->winErr = rsp.error;
    req->response = str::Dup(ToStr(rsp.data));
    auto fn = MkFunc0<PostRequest>(PostRequestFinished, req);
    uitask::Post(fn, "SelectionHandlerPostFinished");
}

// Runs a POST selection handler. Returns immediately; the request runs on a
// background thread and reports the outcome in a notification.
void SelectionHandlerPost(WindowTab* tab, Str url, Str bodyPattern, Str contentType, Str headers, Str selection) {
    auto* req = new PostRequest();
    req->tab = tab;
    req->url = str::Dup(url);
    // no Body given: send the selection as-is. That is the least surprising
    // thing for "just post my text somewhere", and pairs with the default
    // text/plain content type
    Str pattern = str::IsEmptyOrWhiteSpace(bodyPattern) ? Str(kSelectionStr) : bodyPattern;
    req->body = str::Dup(ExpandSelectionVarsTemp(pattern, selection, false));
    Str ct = str::IsEmptyOrWhiteSpace(contentType) ? StrL("text/plain; charset=utf-8") : contentType;
    req->contentType = str::Dup(ct);
    req->headers = str::Dup(headers);

    auto fn = MkFunc0<PostRequest>(PostRequestThread, req);
    RunAsync(fn, "SelectionHandlerPost");
}

//--- Method = POST-VIA-BROWSER (self-submitting html form)

static void HtmlAttrEscape(str::Builder& b, Str s) {
    const char* p = s.s;
    int n = len(s);
    for (int i = 0; i < n; i++) {
        char c = p[i];
        switch (c) {
            case '&':
                b.Append("&amp;");
                break;
            case '<':
                b.Append("&lt;");
                break;
            case '>':
                b.Append("&gt;");
                break;
            case '"':
                b.Append("&quot;");
                break;
            case '\'':
                b.Append("&#39;");
                break;
            // a literal newline inside an attribute value does survive, but
            // only because browsers are lenient about it - be explicit
            case '\n':
                b.Append("&#10;");
                break;
            case '\r':
                b.Append("&#13;");
                break;
            default:
                b.AppendChar(c);
        }
    }
}

// Runs a POST-VIA-BROWSER selection handler by writing a temp html page with an
// auto-submitting form and opening it in the default browser.
void SelectionHandlerPostViaBrowser(WindowTab* tab, Str url, Str bodyPattern, Str selection) {
    // The body is a form-encoded template, e.g. "text=${selection}&lang=${userlang}".
    // Split it *before* substituting so a selection containing & or = can't
    // invent extra form fields.
    Str pattern = str::IsEmptyOrWhiteSpace(bodyPattern) ? StrL("text=${selection}") : bodyPattern;

    str::Builder html;
    html.Append("<!doctype html><html><head><meta charset=\"utf-8\"><title>SumatraPDF</title></head>\n");
    html.Append(
        "<body onload=\"document.forms[0].submit()\">\n<form method=\"post\" accept-charset=\"utf-8\" action=\"");
    HtmlAttrEscape(html, url);
    html.Append("\">\n");

    StrVec fields;
    Split(&fields, pattern, StrL("&"), true);
    for (Str field : fields) {
        TempStr name = str::DupTemp(field);
        TempStr valuePattern = StrL("");
        int idx = str::IndexOfChar(name, '=');
        if (idx >= 0) {
            valuePattern = str::DupTemp(Str(name.s + idx + 1, len(name) - idx - 1));
            name = str::DupTemp(Str(name.s, idx));
        }
        if (str::IsEmptyOrWhiteSpace(name)) {
            continue;
        }
        TempStr value = ExpandSelectionVarsTemp(valuePattern, selection, false);
        html.Append(R"(<input type="hidden" name=")");
        HtmlAttrEscape(html, name);
        html.Append("\" value=\"");
        HtmlAttrEscape(html, value);
        html.Append("\">\n");
    }
    html.Append("</form>\n<noscript><button type=\"submit\">Continue</button></noscript>\n</body></html>\n");

    TempStr dir = GetTempDirTemp();
    if (str::IsEmptyOrWhiteSpace(dir)) {
        ShowSelectionHandlerNotification(tab, _TRA("Couldn't create a temporary file"), true);
        return;
    }
    TempStr name = fmt("SumatraPDF-post-%d.html", (int)GetCurrentProcessId());
    TempStr path = path::JoinTemp(dir, name);
    if (!file::WriteFile(path, ToStr(html))) {
        ShowSelectionHandlerNotification(tab, _TRA("Couldn't create a temporary file"), true);
        return;
    }
    // the file stays behind after the browser reads it; it's overwritten on the
    // next use and lives in the temp directory, which the system cleans up
    LaunchFileShell(path, Str(), StrL("open"));
}
