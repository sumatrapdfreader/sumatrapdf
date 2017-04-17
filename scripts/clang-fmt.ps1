# This requires clang-format to be installed and in %PATH%
# http://llvm.org/releases/download.html
# it's part of clang installer

Remove-Item src\*.bak,src\*.tmp

$files = "src\ParseCommandLine*","src\Tests*","src\Print*","src\Favorites.*","src\Tabs.*","src\TabInfo.*","src\mui\SvgPath.*","src\Notifications.*","src\EbookController.*","src\Menu.*","src\Theme.*","src\wingui\TabsCtrl.*"

foreach ($file in $files) {
    $files2 = Get-ChildItem $file
    foreach ($file2 in $files2) {
        Write-Host $file2
        clang-format.exe -i -style=file $file2
    }
}

Remove-Item src\*.bak,src\*.tmp
