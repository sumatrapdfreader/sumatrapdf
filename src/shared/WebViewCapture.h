/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ICoreWebView2;

bool WebView2CapturePng(ICoreWebView2* webview, HWND hwnd, const Func1<Str>& done);
