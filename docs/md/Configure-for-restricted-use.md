# Configure for restricted use

SumatraPDF can be configured for restricted use.

Restricted mode is useful if you want to use SumatraPDF as a bundled viewer for your program's documentation or in kiosk mode.

In restricted mode, some actions that are not appropriate in such contexts are disabled:

- opening new files
- launching URLs from within PDF documents
- text and image selection
- printing
- changing default settings
- saving to disk
- automatic and manual update checks
- a history of recently opened files
- TeX preview support
- registering as a default PDF viewer
- opening with Adobe Acrobat
- emailing PDFs

To restrict SumatraPDF, put the `sumatrapdfrestrict.ini` file in the same directory as `SumatraPDF.exe`.

Here is the [full documentation for the available options](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/docs/sumatrapdfrestrict.ini).

To disable only auto-detected URL, email, and plain-text DOI links in PDF text (not embedded hyperlinks), use `DisableAutoLinks` in normal [advanced settings](Advanced-options-settings.md) instead — see [Hyperlinks](Hyperlinks.md).
