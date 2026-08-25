# Set as default PDF viewer

SumatraPDF registers itself for all [supported file types](Supported-document-formats.md) (`.pdf`, `.png`, etc.).

If there are no other applications registered for a given extension, you can open the file in SumatraPDF simply by double-clicking the file in File Explorer.

If there is more than one application, you have to explicitly tell Windows 10+ to make SumatraPDF the default application.

## Using File Explorer

In File Explorer:

- find a `.pdf` file (or any other supported file type)
- right-click to open the context menu
- select `Open with` and then `Choose another app`

![Explorer Menu Open With](img/explorer_menu_open_with.png)

From the list, choose `SumatraPDF` and check `Always use this app to open .pdf files`:

![Choose App](img/choose_app.png)

## Using Default apps system settings

This is based on the latest Windows 11 build at the time of writing.

Unfortunately the details differ between Windows updates.

Open the `Default apps` section of the Settings app. For example, press the `Windows logo` key to open system-wide search, type `default apps`, and click the `Default apps` search result.

![Default apps](img/default-apps.png)

In Default apps, type `.pdf` as the file extension:

![Default apps Settings](img/settings-app.png)

Click the current default PDF application (`Microsoft Edge` in this example) and select `SumatraPDF`:

You can do that for other file formats.
