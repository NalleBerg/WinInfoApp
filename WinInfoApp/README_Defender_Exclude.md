Norsk forklaring etter den engelske

---

English:

This file explains how to exclude the packaged WinInfoApp folder from Microsoft Defender (Windows Security). Excluding the folder prevents Defender from flagging the application on launch, but be aware that adding exclusions reduces Defender protection for the excluded files/folder. Only exclude software you trust.

Quick GUI steps (Windows 10 / 11):
1. Open "Windows Security" (type "Windows Security" in Start).
2. Click "Virus & threat protection".
3. Click "Manage settings" under "Virus & threat protection settings".
4. Scroll to "Exclusions" and click "Add or remove exclusions".
5. Click "Add an exclusion", choose "Folder" and browse to the packaged app folder, for example:

   C:\Users\<YourUser>\Documents\C++\Workspace\WinInfoApp\WinInfoApp

   Select that folder and confirm. This excludes the whole package directory and its DLLs from real-time scanning.

PowerShell (admin) method:
- Open an elevated PowerShell (Run as Administrator) and run:

  Add-MpPreference -ExclusionPath "C:\Users\<YourUser>\Documents\C++\Workspace\WinInfoApp\WinInfoApp"

- To exclude only the executable file instead of the whole folder, run:

  Add-MpPreference -ExclusionProcess "C:\Users\<YourUser>\Documents\C++\Workspace\WinInfoApp\WinInfoApp\WinInfoApp.exe"

- To list current exclusions:

  Get-MpPreference | Select-Object -Property ExclusionProcess,ExclusionPath,ExclusionExtension,ExclusionIpAddress

- To remove an exclusion (example for path):

  Remove-MpPreference -ExclusionPath "C:\Users\<YourUser>\Documents\C++\Workspace\WinInfoApp\WinInfoApp"

Notes and recommendations:
- Administrative privileges are required to run the PowerShell commands.
- Excluding the folder disables Defender scanning for those files — only do this for software you control and trust.
- If Windows Defender or SmartScreen still warns, consider code-signing your EXE and DLLs with a trusted code-signing certificate. Signing greatly reduces false-positive warnings from Defender and SmartScreen.
- For distribution, build an installer (MSI or EXE), sign the installer, and optionally submit files to Microsoft for analysis/whitelisting.
- If Defender flags the app after an update, the exclusion may still apply, but Defender heuristic updates can change behavior; signing remains the most reliable mitigation.

---

Norsk:

Denne filen forklarer hvordan du kan legge den pakkede WinInfoApp-mappen til unntak i Microsoft Defender (Windows Security). Å legge til unntak forhindrer at Defender merker programmet ved oppstart, men merk at et unntak reduserer Defender-beskyttelsen for de utelatte filene/mappen. Legg kun til unntak for programvare du stoler på.

GUI-trinn (Windows 10 / 11):
1. Åpne "Windows Security" (søk etter "Windows Security" i Start).
2. Klikk "Virus & threat protection".
3. Klikk "Manage settings" under "Virus & threat protection settings".
4. Bla ned til "Exclusions" og klikk "Add or remove exclusions".
5. Klikk "Add an exclusion", velg "Folder" og naviger til den pakkede app-mappen, for eksempel:

   C:\Users\<DittBrukernavn>\Documents\C++\Workspace\WinInfoApp\WinInfoApp

   Velg mappen og bekreft. Dette legger hele pakkemappen og dens DLL-er utenfor sanntidsskanning.

PowerShell-metode (administrator):
- Åpne en forhøyet PowerShell (Kjør som administrator) og kjør:

  Add-MpPreference -ExclusionPath "C:\Users\<DittBrukernavn>\Documents\C++\Workspace\WinInfoApp\WinInfoApp"

- For å unnta kun kjørbar fil i stedet for hele mappen, kjør:

  Add-MpPreference -ExclusionProcess "C:\Users\<DittBrukernavn>\Documents\C++\Workspace\WinInfoApp\WinInfoApp\WinInfoApp.exe"

- For å liste nåværende unntak:

  Get-MpPreference | Select-Object -Property ExclusionProcess,ExclusionPath,ExclusionExtension,ExclusionIpAddress

- For å fjerne et unntak (eksempel for sti):

  Remove-MpPreference -ExclusionPath "C:\Users\<DittBrukernavn>\Documents\C++\Workspace\WinInfoApp\WinInfoApp"

Anbefalinger og merknader:
- Du må ha administrative rettigheter for å kjøre PowerShell-kommandoene.
- Et unntak deaktiverer Defender-skanning for de aktuelle filene — gjør dette kun for programvare du kontrollerer og stoler på.
- Hvis Windows Defender eller SmartScreen fortsatt gir advarsler, bør du signere EXE og DLL-er med et betrodd kode-signatursertifikat. Kodesignering reduserer falske positiver fra Defender og SmartScreen betraktelig.
- For distribusjon: lag en signert installasjonsfil (MSI/EXE), signer den, og vurder å sende filene til Microsoft for whitelisting.
- Hvis Defender varsler etter en oppdatering av programmet kan heuristiske regler endres; kodesignering er den mest pålitelige løsningen.

---

File location: place this README in the package folder next to `WinInfoApp.exe` (already created here).
