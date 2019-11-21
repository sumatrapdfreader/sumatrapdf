# [Documentation](/docs/) : How we store settings

Persisted data is stored in SumatraPDF-settings.txt file. In portable version the file is stored in the same directory as SumatraPDF executable. In non-portable version, it's in `%APPDATA%\SumatraPDF` directory.

Starting with version 1.6 we also persist thumbnails for "Frequently read" list. They are stored in subdirectory `sumatrapdfcache` as .png files.

See [www.sumatrapdfreader.org/settings.html](http://www.sumatrapdfreader.org/settings.html) for more information about all the settings which are currently stored and [scripts/gen_settingsstructs.py](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/scripts/gen_settingsstructs.py) for what to change in order to add new settings.