@rem flags:
@rem   -release
@rem   -prerelease
@rem   -upload
@rem   -no-clean-check

go run .\tools\build\analyze.go .\tools\build\cmd.go .\tools\build\main.go .\tools\build\s3.go .\tools\build\util.go %1 %2 %3 %4 %5
