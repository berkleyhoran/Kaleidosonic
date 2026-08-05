; Inno Setup script for Kaleidosonic -- dark-themed wizard.
; Build with: ISCC.exe Installer\Kaleidosonic.iss   (after a Release build)
; Produces Installer\Output\KaleidosonicSetup.exe -- a single installer
; anyone can run: it puts the VST3 where every DAW looks for it
; (C:\Program Files\Common Files\VST3) and optionally installs the
; Standalone app with a Start Menu shortcut.

#define AppName "Kaleidosonic"
#define AppVersion "0.1.0"
#define BuildDir "..\build\Kaleidosonic_artefacts\Release"

[Setup]
AppId={{7E1B7C52-9C1A-4B7E-9D8A-3F2A61C04D19}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Kaleidosonic
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=KaleidosonicSetup
OutputDir=Output
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
; Installing into Common Files\VST3 needs admin rights.
PrivilegesRequired=admin
WizardStyle=modern
WizardSizePercent=110
SetupIconFile=..\Assets\logo.ico
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard_small.bmp
WizardImageStretch=yes

[Types]
Name: "full"; Description: "VST3 plugin + Standalone app"
Name: "vst3only"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (for Ableton/any DAW)"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full custom

[Files]
; The .vst3 is a bundle folder -- copy it whole into the system VST3 dir.
Source: "{#BuildDir}\VST3\Kaleidosonic.vst3\*"; DestDir: "{commoncf64}\VST3\Kaleidosonic.vst3"; \
    Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#BuildDir}\Standalone\Kaleidosonic.exe"; DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Kaleidosonic.exe"; Components: standalone
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Kaleidosonic.exe"; Description: "Launch {#AppName}"; \
    Components: standalone; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel1=Welcome to [name]
WelcomeLabel2=The audio-reactive fractal visualizer.%n%nThis will install the [name] VST3 plugin (and optionally the standalone app) on your computer.%n%nDrop it on any track, press play, and dive.
FinishedHeadingLabel=You're in.
FinishedLabel=[name] is installed. In your DAW, rescan plugins if it doesn't show up right away (Ableton: Preferences > Plug-Ins > Rescan).

[Code]
// Dark theme: Inno has no native dark mode, so recolor the wizard's
// surfaces and labels by hand. Buttons/checkboxes keep system rendering
// (their faces can't be restyled from here), which reads fine against the
// dark chrome.
procedure DarkenControl(Parent: TWinControl);
var
  I: Integer;
  C: TControl;
begin
  for I := 0 to Parent.ControlCount - 1 do
  begin
    C := Parent.Controls[I];
    if C is TNewStaticText then
    begin
      TNewStaticText(C).Font.Color := $00EDE6F2; // near-white lavender
      TNewStaticText(C).Color := $00140C10;
    end
    else if C is TLabel then
    begin
      TLabel(C).Font.Color := $00EDE6F2;
      TLabel(C).Transparent := True;
    end
    else if C is TNewCheckListBox then
    begin
      TNewCheckListBox(C).Color := $00241820;
      TNewCheckListBox(C).Font.Color := $00EDE6F2;
    end
    else if C is TNewEdit then
    begin
      TNewEdit(C).Color := $00241820;
      TNewEdit(C).Font.Color := $00EDE6F2;
    end
    else if C is TNewMemo then
    begin
      TNewMemo(C).Color := $00241820;
      TNewMemo(C).Font.Color := $00EDE6F2;
    end
    else if C is TWinControl then
    begin
      DarkenControl(TWinControl(C));
    end;
  end;
end;

procedure InitializeWizard();
begin
  // BGR hex: deep near-black purple chrome.
  WizardForm.Color := $00140C10;
  WizardForm.MainPanel.Color := $00140C10;
  WizardForm.InnerPage.Color := $00140C10;
  WizardForm.TasksList.Color := $00241820;
  WizardForm.PageNameLabel.Font.Color := $00F2E6ED;
  WizardForm.PageDescriptionLabel.Font.Color := $00B49CC8;
  WizardForm.Bevel.Visible := False;
  WizardForm.Bevel1.Visible := False;
  WizardForm.WelcomePage.Color := $00140C10;
  WizardForm.FinishedPage.Color := $00140C10;
  WizardForm.WelcomeLabel1.Font.Color := $00F2E6ED;
  WizardForm.WelcomeLabel1.Font.Size := 14;
  WizardForm.WelcomeLabel1.Font.Style := [fsBold];
  WizardForm.WelcomeLabel2.Font.Color := $00D8C8E4;
  WizardForm.FinishedHeadingLabel.Font.Color := $00F2E6ED;
  WizardForm.FinishedHeadingLabel.Font.Size := 14;
  WizardForm.FinishedLabel.Font.Color := $00D8C8E4;
  DarkenControl(WizardForm.InnerPage);
end;
