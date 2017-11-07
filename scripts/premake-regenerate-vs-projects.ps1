if (!(Test-Path ".\\bin\\premake5.exe")) {
  Write-Output "premake5.exe is not in bin directory. Download from https://premake.github.io/download.html and put in bin directory."
  exit
}

bin\premake5.exe vs2017
