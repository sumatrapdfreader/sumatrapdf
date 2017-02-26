
@rem builds and sings 32-bit release version of MakeLZSA.exe so that
@rem we can add it to bin\

go run .\tools\build\analyze.go .\tools\build\cmd.go .\tools\build\main.go .\tools\build\s3.go .\tools\build\util.go -build-makelzsa

dir rel\MakeLZSA.exe
