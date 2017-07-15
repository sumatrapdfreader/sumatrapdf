# This requires clang-format to be installed and in %PATH%
# http://llvm.org/releases/download.html
# it's part of clang installer

Remove-Item src\*.bak, src\*.tmp

$files = "src\Caption.*",
"src\Canvas.*",
"src\Colors.*",
"src\EbookController.*",
"src\Favorites.*",
"src\Menu.*",
"src\Notifications.*",
"src\ParseCommandLine*",
"src\Print*",
"src\SumatraDialogs.*",
"src\TabInfo.*",
"src\Tabs.*",
"src\Theme.*",
"src\Tests*",
"src\TextSearch*",
"src\mui\SvgPath.*",
"src\utils\FileWatcher.*",
"src\utils\HttpUtil.*",
"src\utils\Scoped.*",
"src\utils\StrUtil.*",
"src\utils\WinUtil.*",
"src\wingui\*",
"src\installer\Install.cpp",
"src\installer\Installer.cpp",
"src\installer\Installer.h",
"src\installer\Uninstall.cpp"

foreach ($file in $files) {
  $files2 = Get-ChildItem $file
  foreach ($file2 in $files2) {
    Write-Host $file2
    clang-format.exe -i -style=file $file2
  }
}

Get-ChildItem -Recur -Filter "*.tmp" | Remove-Item
Get-ChildItem -Recur -Filter "*.bak" | Remove-Item
