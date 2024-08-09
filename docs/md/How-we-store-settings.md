# How we store settings

## Where we store settings

Persisted data is stored in `SumatraPDF-settings.txt` file.

In portable version the file is stored in the same directory as SumatraPDF executable. In non-portable version, it's in `%LOCALAPPDATA%\SumatraPDF` directory.

Starting with version 1.6 we also persist thumbnails for "Frequently read" list. They are stored in subdirectory `sumatrapdfcache` as `.png` files.

See [https://www.sumatrapdfreader.org/settings/settings](https://www.sumatrapdfreader.org/settings/settings) for information about all the settings.
