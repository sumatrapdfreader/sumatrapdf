; TODO: 
; - assign myself with PDF via registry, possibly a checkbox to enable/disable this
; - a way to run the app after installation is done
; - restore PDF association on uninstall. How to do it? Just assume acrobat?
;   execute an app (e.g. myself with -uninstall switch) that would restore
;   PDF associations?

Name "SumatraPDF"
OutFile "SumatraPDF-install.exe"
InstallDir $PROGRAMFILES\SumatraPDF

SetCompress off

; Registry key to check for directory (so if you install again, it will  overwrite the old one automatically)
InstallDirRegKey HKLM "Software\SumatraPDF" "Install_Dir"

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "SumatraPDF"

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  File /oname=SumatraPDF.exe "..\obj-rel\SumatraPDF.exe"

  SectionIn RO
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\SumatraPDF "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "DisplayName" "Sumatra PDF reader"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "NoRepair" 1
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts" RO
  SectionIn RO

  CreateDirectory "$SMPROGRAMS\SumatraPDF"
  CreateShortCut "$SMPROGRAMS\SumatraPDF\Uninstall.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\SumatraPDF\SumatraPDF.lnk" "$INSTDIR\SumatraPDF.exe" "" "$INSTDIR\SumatraPDF.exe" 0
SectionEnd

; Uninstaller
Section "Uninstall"

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF"
  DeleteRegKey HKLM SOFTWARE\SumatraPDF

  ; Remove files and uninstaller
  Delete $INSTDIR\Uninstall.exe
  Delete $INSTDIR\SumatraPDF.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\SumatraPDF\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\SumatraPDF"
  RMDir "$INSTDIR"

SectionEnd
