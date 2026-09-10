# WebView2

`WebView2.h`, `WebView2EnvironmentOptions.h` and `LICENSE.txt` are from the
Microsoft.Web.WebView2 NuGet package, version 1.0.4022.49. Update them by
downloading a newer package and copying `build/native/include`.

`WebView2Loader.cpp` is ours. It replaces the package's
`WebView2LoaderStatic.lib`, so we don't have to carry per-architecture binaries.
It is derived from the loader in [wry](https://github.com/kjk/gpui-kit-cpp-dist/tree/main/extras/wry)
(`FindRuntime` and friends): it locates the installed runtime via the EdgeUpdate
registry keys or an MSIX package dependency, honoring the same environment
variable and group policy overrides as Microsoft's loader, then calls
`CreateWebViewEnvironmentWithOptionsInternal` in `EmbeddedBrowserWebView.dll`.
