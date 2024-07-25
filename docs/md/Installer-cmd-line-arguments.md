# Installer cmd-line arguments

To get list of options run the installer with `-help`.

Available options:

- `-install` : this triggers installation
- `-s`, `-silent` : silent installation, doesn't show UI
- `-d <directory>` e.g. `Sumatra-install.exe -install -d "c:\Sumatra PDF"`
    set directory where program is installed. The default is `%LOCALAPPDATA%\SumatraPDF` or `%PROGRAMFILES%\SumatraPDF` with `-all-users`
- `-x` : don't  install, extract the files
    extracts files to current directory or directory provided with `-d` option
- `-with-filter` : install search filter
- `-with-preview` : install shell preview for PDF files
- `-uninstall` : uninstalls SumatraPDF

**Ver 3.2+**

- `-log`
writes installation log to `%LOCALAPPDATA%\sumatra-install-log.txt`. At the end of installation will open the log file in notepad.

**Ver 3.4+**

- `-all-users` : installs system-wide, for all users
installs to `%PROGRAMFILES%\SumatraPDF` and writes to `HKLM` registry

**Ver 3.6+**

- `-fast-install` : automatically starts installation with default options, starts the app when installation is finished
