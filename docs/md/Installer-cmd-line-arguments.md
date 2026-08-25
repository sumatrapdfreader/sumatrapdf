# Installer cmd-line arguments

To get a list of options, run the installer with `-help`.

Available options:

- `-install` : triggers installation
- `-s`, `-silent` : performs a silent installation without showing the UI
- `-d <directory>` e.g. `Sumatra-install.exe -install -d "c:\Sumatra PDF"`
  sets the directory where the program is installed. The default is `%LOCALAPPDATA%\SumatraPDF`, or `%PROGRAMFILES%\SumatraPDF` with `-all-users`
- `-x` : don't install, extract the files
  extracts files to the current directory, or to `-d <directory>` if specified. When the destination is this executable's own directory, `SumatraPDF.exe` is left in place and only the payload (`libsumatrapdf.dll` and the other installation files) is unpacked. Errors are printed to the console if one is already attached.
- `-with-filter` : install search filter
- `-with-preview` : install shell preview for PDF files
- `-uninstall` : uninstalls SumatraPDF

**Ver 3.2+**

- `-log`
  writes the installation log to `%LOCALAPPDATA%\sumatra-install-log.txt`. At the end of the installation, it opens the log file in Notepad.

**Ver 3.4+**

- `-all-users` : installs system-wide, for all users
  installs to `%PROGRAMFILES%\SumatraPDF` and writes to the `HKLM` registry

**Ver 3.6+**

- `-fast-install` : internal for auto-update, automatically starts installation with default options, starts the app when installation is finished
