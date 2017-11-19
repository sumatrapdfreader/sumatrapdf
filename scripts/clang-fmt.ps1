# This requires clang-format to be installed and in %PATH%
# http://llvm.org/releases/download.html
# it's part of clang installer

Remove-Item src\*.bak, src\*.tmp

$files =
#"src\AppPrefs.*",
#"src\AppTools.*",
#"src\AppUtil.*",
"src\Canvas.*",
"src\Caption.*",
#"src\ChmDoc.*",
#"src\ChmModel.*",
"src\Colors.*",
#"src\CrashHandler.*",
#"src\DisplayModel.*",
#"src\DjVuEngine.*",
"src\Doc.*",
"src\EbookController.*",
#"src\EbookControls.*",
"src\EbookDoc.*",
#"src\EbookEngine.*",
#"src\EbookFormatter.*",
#"src\EngineDump.*",
"src\EngineManager.*",
"src\ExternalViewers.*",
"src\Favorites.*",
#"src\FileHistory.*",
"src\FileModifications.*",
#"src\FileThumbnails.*",
#"src\GlobalPrefs.*",
#"src\HtmlFormatter.*",
"src\ImagesEngine.*",
"src\Menu.*",
#"src\MobiDoc.*",
#"src\MuiEbookPageDef.*",
"src\Notifications.*",
#"src\PagesLayoutDef*",
"src\ParseCommandLine*",
#"src\PdfCreator.*",
"src\PdfEngine.*",
#"src\PdfSync.*",
"src\Print*",
#"src\PsEngine.*",
#"src\RenderCache.*",
"src\Search.*",
"src\Selection.*",
#"src\SetiingsStructs.*",
#"src\StressTesting.*",
#"src\SumatraAbout.*",
#"src\SumatraAbout2.*",
"src\SumatraDialogs.*",
#"src\SumatraPDF.*",
#"src\SumatraProperties.*",
#"src\SumatraStartup.*",
"src\TabInfo.*",
#"src\TableOfContents.*",
"src\Tabs.*",
#"src\Tester.*",
"src\Tests*",
"src\TextSearch*",
#"src\TextSelection.*",
"src\Theme.*",
#"src\Toolbar.*",
#"src\UnitTests.*",
#"src\WindowInfo.*",
"src\mui\SvgPath.*",
"src\utils\ArchUtil.*",
"src\utils\FileWatcher.*",
"src\utils\HttpUtil.*",
"src\utils\HtmlWindow.*",
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
