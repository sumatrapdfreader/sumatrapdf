# How we store settings

## Where we store settings

Persisted data is stored in `SumatraPDF-settings.txt` file.

In portable version the file is stored in the same directory as SumatraPDF executable. In non-portable version, it's in `%LOCALAPPDATA%\SumatraPDF` directory.

Starting with version 1.6 we also persist thumbnails for "Frequently read" list. They are stored in subdirectory `sumatrapdfcache` as `.png` files.

See [https://www.sumatrapdfreader.org/settings/settings](https://www.sumatrapdfreader.org/settings/settings) for information about all the settings.

## How to add more settings

Don’t manually edit `Settings.h` file. Settings and their html documentations are generated via Go script.

To add more settings:

- you need to have Go installed
- edit [https://github.com/sumatrapdfreader/sumatrapdf/blob/master/do/settings_def.go](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/do/settings_def.go) to add/remove settings
- run `doit.bat -gen-settings` to regenerate the files
- checkin regenerated files