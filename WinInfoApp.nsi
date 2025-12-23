!include MUI2.nsh

!define PRODUCT_NAME "WinInfoApp"
!define COMPANY_NAME "WinInfoAppAuthor"

!ifdef OUTNAME
!define OUTPUT_FILE "${OUTNAME}.exe"
!else
!define OUTPUT_FILE "WinInfoApp-Setup.exe"
!endif

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
ShowInstDetails show
ShowUninstDetails show

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "GPLv2.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Section "Install"
  SetOutPath "$INSTDIR"
  File /r "*"
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\WinInfoApp.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\WinInfoApp.exe"
  RMDir /r "$INSTDIR"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
SectionEnd
