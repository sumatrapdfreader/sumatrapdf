# This requires clang-format to be installed and in %PATH%
# http://llvm.org/releases/download.html
# it's part of clang installer

Remove-Item src\*.bak, src\*.tmp

$files = "src\Canvas.*",
  "src\Colors.*",
  "src\EbookController.*",
  "src\Favorites.*",
  "src\Menu.*",
  "src\Notifications.*",
  "src\ParseCommandLine*",
  "src\Print*",
  "src\TabInfo.*",
  "src\Tabs.*",
  "src\Theme.*",
  "src\Tests*",
  "src\mui\SvgPath.*",
  "src\utils\FileWatcher.*",
  "src\wingui\*"

foreach ($file in $files) {
  $files2 = Get-ChildItem $file
  foreach ($file2 in $files2) {
    Write-Host $file2
    clang-format.exe -i -style=file $file2
  }
}

Remove-Item src\*.bak, src\*.tmp
