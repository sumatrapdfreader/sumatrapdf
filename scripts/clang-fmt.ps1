# This requires clang-format to be installed and in %PATH%
# http://llvm.org/releases/download.html
# it's part of clang installer

Remove-Item src\*.bak, src\*.tmp

$files =
"src\*.cpp",
"src\*.h",
"src\mui\*.cpp",
"src\mui\*.h",
"src\utils\*.cpp",
"src\utils\*.h",
"src\utils\tests\*.cpp",
"src\utils\tests\*.h",
"src\wingui\*.cpp",
"src\wingui\*.h",
"src\installer\*.cpp",
"src\installer\*.h",
"src\tools\*.cpp"
"src\tools\*.h"

$whiteList =
"resource.h",
"Version.h",
"Trans_sumatra_txt.cpp",
"Trans_installer_txt.cpp"

function isWhiteListed($name) {
  $name = $name.ToLower()
	foreach ($item in $whiteList) {
		$inList = $item.ToLower().EndsWith($name)
		if ($inList) {
			return $inList
		}
	}
	return $false
}

foreach ($file in $files) {
  $files2 = Get-ChildItem $file
  foreach ($file2 in $files2) {
    if (isWhiteListed($file2.Name)) {
      Write-Host "Skipping $file2"
    } else {
      Write-Host $file2
      clang-format.exe -i -style=file $file2
    }
  }
}

Get-ChildItem -Recur -Filter "*.tmp" | Remove-Item
Get-ChildItem -Recur -Filter "*.bak" | Remove-Item
