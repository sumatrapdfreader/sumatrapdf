; Use modern visuals for the Installer
!addplugindir bin
!include MUI2.nsh
!include FileFunc.nsh
XpStyle on

!define APP "SumatraPDF"
!define EXE "SumatraPDF.exe"
!define REG_PATH_UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"

Name "SumatraPDF"
OutFile "SumatraPDF-${SUMVER}-install.exe"

; Registry key to check for directory (so if you install again, it will overwrite the old one automatically)
InstallDirRegKey HKLM "Software\${APP}" "Install_Dir"
InstallDir $PROGRAMFILES\${APP}

;; Uncomment to replace the NSIS icon with the SumatraPDF one:
; !define MUI_ICON SumatraPDF.ico
!define MUI_COMPONENTSPAGE_NODESC

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Add the option to launch SumatraPDF right after the installation
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch SumatraPDF"
;; Remove to actually check the option:
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "SumatraPDF" SecMain
    ; Prevent this section from being unselected
    SectionIn RO

    ; kill the process so that installer can over-write with new executable
    Processes::KillProcessAndWait "${EXE}"
    ; empirically we need to sleep a while before the file can be overwritten
    Sleep 1000

    ; Copy the main executable, the CJK fallback font and the uninstaller
    SetOutPath $INSTDIR
    File /oname=SumatraPDF.exe "builds\${SUMVER}\SumatraPDF-${SUMVER}.exe"
    File "..\mupdf\fonts\droid\DroidSansFallback.ttf"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Always create the link for all users
    ; (note: the Windows HIG says not to create an uninstallation link)
    SetShellVarContext all
    CreateShortCut "$SMPROGRAMS\SumatraPDF.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0

    ; Write installation path into the registry
    WriteRegStr HKLM SOFTWARE\${APP} "Install_Dir" "$INSTDIR"

    ; Write uninstall keys for Windows
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "DisplayName"     "${APP}"
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "DisplayVersion"  ${SUMVER}
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "DisplayIcon"    "$INSTDIR\${EXE}"
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "Publisher"      "Krzysztof Kowalczyk"
    WriteRegStr   HKLM "${REG_PATH_UNINST}" "UrlInfoAbout"   "http://blog.kowalczyk.info/software/sumatrapdf/"
    WriteRegDWORD HKLM "${REG_PATH_UNINST}" "NoModify"        1
    WriteRegDWORD HKLM "${REG_PATH_UNINST}" "NoRepair"        1

    ; based on http://stackoverflow.com/questions/317647/using-the-estimatedsize-value-inside-a-program-uninstall-key-to-correctly-display
    ; get cumulative size of all files in and under install dir
    ; report the total in KB (decimal)
    ; place the answer into $0  ($1 and $2 get other info we don't care about)
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    
    ; Convert the decimal KB value in $0 to DWORD
    ; put it right back into $0
    IntFmt $0 "0x%08X" $0
    
    ; Create/Write the reg key with the dword value
    WriteRegDWORD HKLM "${REG_PATH_UNINST}" "EstimatedSize" "$0"

SectionEnd

; Optional section (unselected by default)
Section /o "Use SumatraPDF as my default PDF reader" SecDefault
    Exec '"$INSTDIR\${EXE}" -register-for-pdf'
SectionEnd

; Setup the Installer sections at startup
Function .onInit
    ; Check the option, if no other application is set as default PDF reader
    ClearErrors
    ReadRegStr $0 HKCR ".pdf" ""
    ${If} ${Errors}
    ${OrIf} $0 == "${APP}"
        SectionSetFlags ${SecDefault} ${SF_SELECTED}
    ${EndIf}
FunctionEnd

Section "Uninstall"
    ; kill the process so that we can delete the executable
    Processes::KillProcessAndWait "${EXE}"
    ; empirically we need to sleep a while before the file can be overwritten
    Sleep 1000

    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"
    DeleteRegKey HKLM "Software\${APP}"

    ; Try to restore the previous default PDF reader (for all users)
    ClearErrors
    ReadRegStr $0 HKLM "Software\Classes\${APP}" "previous.pdf"
    ${IfNot} ${Errors}
        WriteRegStr HKLM "Software\Classes\.pdf" "" $0
    ${Else}
        ClearErrors
        ReadRegStr $0 HKLM "Software\Classes\.pdf" ""
        ${IfNot} ${Errors}
        ${AndIf} $0 == "SumatraPDF"
            DeleteRegValue HKLM "Software\Classes\.pdf" ""
        ${EndIf}
    ${EndIf}
    DeleteRegKey HKLM "Software\Classes\${APP}"

    ; Try to restore the previous default PDF reader (for the current user)
    ClearErrors
    ReadRegStr $0 HKCU "Software\Classes\${APP}" "previous.pdf"
    ${IfNot} ${Errors}
        WriteRegStr HKCU "Software\Classes\.pdf" "" $0
    ${Else}
        ClearErrors
        ReadRegStr $0 HKCU "Software\Classes\.pdf" ""
        ${IfNot} ${Errors}
        ${AndIf} $0 == "SumatraPDF"
            DeleteRegValue HKCU "Software\Classes\.pdf" ""
        ${EndIf}
    ${EndIf}
    DeleteRegKey HKCU "Software\Classes\${APP}"
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf" "ProgId"

    ; Remove the shortcut, if any
    SetShellVarContext all
    Delete "$SMPROGRAMS\SumatraPDF.lnk"

    ; Remove files and uninstaller
    Delete "$INSTDIR\${EXE}"
    Delete "$INSTDIR\DroidSansFallback.ttf"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
SectionEnd
