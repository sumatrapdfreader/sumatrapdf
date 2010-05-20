; Use modern visuals for the Installer
!include MUI2.nsh
!include FileFunc.nsh
XpStyle on

Name "SumatraPDF"
OutFile "SumatraPDF-${SUMVER}-install.exe"

; Registry key to check for directory (so if you install again, it will overwrite the old one automatically)
InstallDirRegKey HKLM "Software\SumatraPDF" "Install_Dir"
InstallDir $PROGRAMFILES\SumatraPDF

;; Uncomment to replace the NSIS icon with the SumatraPDF one:
; !define MUI_ICON SumatraPDF.ico
!define MUI_COMPONENTSPAGE_NODESC

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Add the option to launch SumatraPDF right after the installation
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_RUN "$INSTDIR\SumatraPDF.exe"
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
	
	; Copy the main executable and the uninstaller
	SetOutPath $INSTDIR
	File /oname=SumatraPDF.exe "builds\${SUMVER}\SumatraPDF-${SUMVER}.exe"
	WriteUninstaller "$INSTDIR\Uninstall.exe"
	
	; Always create the link for all users
	; (note: the Windows HIG says not to create an uninstallation link)
	SetShellVarContext all
	CreateShortCut "$SMPROGRAMS\SumatraPDF.lnk" "$INSTDIR\SumatraPDF.exe" "" "$INSTDIR\SumatraPDF.exe" 0
	
	; Write the installation path into the registry
	WriteRegStr HKLM SOFTWARE\SumatraPDF "Install_Dir" "$INSTDIR"
	
	; Write the uninstall keys for Windows
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "DisplayName" "SumatraPDF"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "DisplayVersion" ${SUMVER}
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF" "NoRepair" 1
SectionEnd

; Optional section (unselected by default)
Section /o "Use SumatraPDF as my default PDF reader" SecDefault
	Exec '"$INSTDIR\SumatraPDF.exe" -register-for-pdf'
SectionEnd

; Setup the Installer sections at startup
Function .onInit
	; Check the option, if no other application is set as default PDF reader
	ClearErrors
	ReadRegStr $0 HKCR ".pdf" ""
	${If} ${Errors}
	${OrIf} $0 == "SumatraPDF"
		SectionSetFlags ${SecDefault} ${SF_SELECTED}
	${EndIf}
FunctionEnd

Section "Uninstall"
	; Remove registry keys
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SumatraPDF"
	DeleteRegKey HKLM "Software\SumatraPDF"
	
	; Try to restore the previous default PDF reader (for all users)
	ClearErrors
	ReadRegStr $0 HKLM "Software\Classes\SumatraPDF" "previous.pdf"
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
	DeleteRegKey HKLM "Software\Classes\SumatraPDF"
	
	; Try to restore the previous default PDF reader (for the current user)
	ClearErrors
	ReadRegStr $0 HKCU "Software\Classes\SumatraPDF" "previous.pdf"
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
	DeleteRegKey HKCU "Software\Classes\SumatraPDF"
	DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.pdf" "ProgId"
	
	; Remove the shortcut, if any
	SetShellVarContext all
	Delete "$SMPROGRAMS\SumatraPDF.lnk"
	
	; Remove files and uninstaller
	Delete "$INSTDIR\SumatraPDF.exe"
	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"
SectionEnd
