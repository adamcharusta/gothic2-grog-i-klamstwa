; Inno Setup 6 script for "Grog i klamstwa".
; Build: iscc packaging\installer.iss /DAppVersion=<v> /DSourceDir=<staged package folder> /DOutputDir=<out>
; Kept ASCII-only on purpose (Inno Setup reads BOM-less files as ANSI).

#ifndef AppVersion
  #define AppVersion "0.0.0-local"
#endif
#ifndef SourceDir
  #error SourceDir must be defined (/DSourceDir=...)
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{0594A339-D37D-43A3-B628-2533E7ABECBC}
AppName=Grog i klamstwa
AppVersion={#AppVersion}
AppPublisher=Adam Charusta
AppPublisherURL=https://github.com/adamcharusta/gothic2-grog-i-klamstwa
DefaultDirName={autopf}\GrogIKlamstwa
DefaultGroupName=Grog i klamstwa
OutputDir={#OutputDir}
OutputBaseFilename=GrogIKlamstwa-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin
LicenseFile={#SourceDir}\LICENSES\LICENSE

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
polish.GothicDirCaption=Folder Gothic II
polish.GothicDirDescription=Gdzie jest zainstalowany Gothic II: Noc Kruka?
polish.GothicDirSubCaption=Wskaz folder gry (ten, w ktorym jest podfolder System). Mod wymaga legalnej kopii gry.
polish.GothicDirInvalid=W tym folderze nie ma pliku System\Gothic2.exe. Wskaz folder Gothic II: Noc Kruka.
english.GothicDirCaption=Gothic II folder
english.GothicDirDescription=Where is Gothic II: Night of the Raven installed?
english.GothicDirSubCaption=Select the game folder (the one containing the System subfolder). A legal copy of the game is required.
english.GothicDirInvalid=System\Gothic2.exe was not found in this folder. Select your Gothic II: Night of the Raven folder.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Mod manifest goes into the game's System folder (OpenGothic looks it up there). Removed on uninstall.
Source: "{#SourceDir}\system\GrogIKlamstwa.ini"; DestDir: "{code:GetGothicDir}\System"; Flags: ignoreversion

[Icons]
Name: "{group}\Grog i klamstwa"; Filename: "{app}\bin\Gothic2Notr.exe"; Parameters: "-g ""{code:GetGothicDir}"" -game:GrogIKlamstwa.ini"; WorkingDir: "{app}\bin"
Name: "{group}\{cm:UninstallProgram,Grog i klamstwa}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Grog i klamstwa"; Filename: "{app}\bin\Gothic2Notr.exe"; Parameters: "-g ""{code:GetGothicDir}"" -game:GrogIKlamstwa.ini"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[UninstallDelete]
Type: files; Name: "{app}\gothic_path.txt"

[Code]
var
  GothicDirPage: TInputDirWizardPage;

function DefaultGothicDir(): String;
var
  Steam: String;
begin
  Steam := ExpandConstant('{commonpf32}') + '\Steam\steamapps\common\Gothic II';
  if DirExists(Steam) then
    Result := Steam
  else
    Result := '';
end;

procedure InitializeWizard();
begin
  GothicDirPage := CreateInputDirPage(wpSelectDir,
    CustomMessage('GothicDirCaption'), CustomMessage('GothicDirDescription'),
    CustomMessage('GothicDirSubCaption'), False, '');
  GothicDirPage.Add('');
  GothicDirPage.Values[0] := DefaultGothicDir();
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = GothicDirPage.ID then
  begin
    if not FileExists(AddBackslash(GothicDirPage.Values[0]) + 'System\Gothic2.exe') then
    begin
      MsgBox(CustomMessage('GothicDirInvalid'), mbError, MB_OK);
      Result := False;
    end;
  end;
end;

function GetGothicDir(Param: String): String;
begin
  Result := RemoveBackslashUnlessRoot(GothicDirPage.Values[0]);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SaveStringToFile(ExpandConstant('{app}\gothic_path.txt'), GetGothicDir(''), False);
end;
