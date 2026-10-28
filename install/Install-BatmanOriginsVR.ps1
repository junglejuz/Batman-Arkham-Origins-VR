<#
  Batman Origins VR installer (Batman: Arkham Origins in PC VR).

  Double-click Install-BatmanOriginsVR.bat. It finds the game, backs up the game's settings, copies the mod into the
  game folder, turns depth of field, motion blur and V-sync off, and makes the "Batman Origins VR" shortcuts.
  Every change is listed in FIX_CHANGES.md; Uninstall-BatmanOriginsVR.bat (copied into the game folder) undoes them.

  Optional: -GameDir "<folder with BatmanOrigins.exe>" -DocsDir "<...\WB Games\Batman Arkham Origins>"
            -DataDir "<mod data folder>" (practice runs; default %LOCALAPPDATA%\Batman_Origins) -Yes -NoShortcuts
#>
param([string]$GameDir, [string]$DocsDir, [string]$DataDir, [switch]$Yes, [switch]$NoShortcuts)

$Mod = @{
    Name        = 'Batman Origins VR'
    Exe         = 'BatmanOrigins.exe'
    SteamFolder = 'Batman Arkham Origins\SinglePlayer\Binaries\Win32'   # under steamapps\common
    DocsFolder  = 'WB Games\Batman Arkham Origins'                       # under Documents
    Gfx         = 'GFXSettings.BatmanArkhamOrigins.xml'
    ProxyMarker = '[batman_origins-proxy]'                               # text inside our xinput1_3.dll
    Backup      = 'BatmanOriginsVR_backup'                               # in the game's top folder
}

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Step([string]$text) { Write-Host ''; Write-Host "  $text" -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host ''; Write-Host "  STOPPED: $text" -ForegroundColor Red; Write-Host ''; exit 1 }
$Here = [IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Path)

# ---- finding things -------------------------------------------------------------------------------
function Find-SteamGameDir {
    $roots = @()
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
        try {
            $p = Get-ItemProperty $key -ErrorAction Stop
            if ($p.SteamPath) { $roots += ($p.SteamPath -replace '/', '\') }
            if ($p.InstallPath) { $roots += $p.InstallPath }
        } catch { }
    }
    $libs = @()
    foreach ($r in ($roots | Select-Object -Unique)) {
        $libs += $r
        $vdf = Join-Path $r 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += ($m.Groups[1].Value -replace '\\\\', '\') }
        }
    }
    foreach ($lib in ($libs | Select-Object -Unique)) {
        $dir = [IO.Path]::Combine($lib, 'steamapps\common\' + $Mod.SteamFolder)
        if (Test-Path -LiteralPath ([IO.Path]::Combine($dir, $Mod.Exe))) { return $dir }
    }
    return $null
}
function Select-Folder([string]$description) {
    $f = New-Object System.Windows.Forms.Form; $f.TopMost = $true
    $d = New-Object System.Windows.Forms.FolderBrowserDialog; $d.Description = $description
    if ($d.ShowDialog($f) -eq [System.Windows.Forms.DialogResult]::OK) { return $d.SelectedPath }
    return $null
}
function Resolve-GameDir([string]$picked) {
    if (-not $picked -or -not (Test-Path -LiteralPath $picked -PathType Container)) { return @() }
    if (Test-Path -LiteralPath (Join-Path $picked $Mod.Exe)) { return @($picked) }
    return @(Get-ChildItem -LiteralPath $picked -Filter $Mod.Exe -Recurse -Depth 5 -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.DirectoryName } | Select-Object -Unique)
}
# Documents can be anywhere (moved, OneDrive): Windows' own lookup first, as the game does.
function Get-DocsCandidates {
    $docs = @([Environment]::GetFolderPath('MyDocuments'))
    try { $docs += [Environment]::ExpandEnvironmentVariables((Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders' -ErrorAction Stop).Personal) } catch { }
    $docs += (Join-Path $env:USERPROFILE 'Documents'), (Join-Path $env:USERPROFILE 'OneDrive\Documents')
    return @($docs | Where-Object { $_ } | Select-Object -Unique | ForEach-Object { Join-Path $_ $Mod.DocsFolder })
}

# =================================================================================================
Write-Host ''
Write-Host '  Batman Origins VR - installer' -ForegroundColor White
Write-Host '  ----------------------------'

# ---- 1. the game ---------------------------------------------------------------------------------
Step '1. Finding Batman: Arkham Origins'
if (-not $GameDir) { $GameDir = Find-SteamGameDir }
while (-not $GameDir -or -not (Test-Path (Join-Path $GameDir $Mod.Exe))) {
    if ($Yes) { Fail 'the game was not found. Run the installer again with -GameDir "<folder with BatmanOrigins.exe>".' }
    Say '  Could not find the game by itself. Please pick its folder (for example ...\steamapps\common\Batman Arkham Origins).' Yellow
    $pick = Select-Folder 'Pick the Batman: Arkham Origins folder'
    if (-not $pick) { Fail 'no game folder was picked.' }
    $found = @(Resolve-GameDir $pick | Where-Object { $_ -like '*SinglePlayer*' })
    if ($found.Count -eq 1) { $GameDir = $found[0] } else { Say '  BatmanOrigins.exe (single player) is not in that folder. Try again.' Yellow }
}
if (Get-Process -Name 'BatmanOrigins' -ErrorAction SilentlyContinue) { Fail 'Batman: Arkham Origins is running. Close it, then run the installer again.' }
$GameDir = [IO.Path]::GetFullPath($GameDir)
$GameRoot = [IO.Path]::GetFullPath((Join-Path $GameDir '..\..\..'))   # ...\Batman Arkham Origins
$Engine = Join-Path $GameRoot 'SinglePlayer\BMGame\Config\BmEngine.ini'
Say "  Game: $GameDir"

if (-not $DocsDir) { $DocsDir = Get-DocsCandidates | Where-Object { Test-Path (Join-Path $_ $Mod.Gfx) } | Select-Object -First 1 }
if (-not $DocsDir -or -not (Test-Path (Join-Path $DocsDir $Mod.Gfx)) -or -not (Test-Path $Engine)) {
    Fail ('the game has not made its settings yet. Start Batman: Arkham Origins once from Steam as normal, wait for the ' +
          'main menu, quit, then run this installer again.')
}
$Gfx = Join-Path $DocsDir $Mod.Gfx
Say "  Game settings: $DocsDir"
if (-not $DataDir) { $DataDir = Join-Path $env:LOCALAPPDATA 'Batman_Origins' }

# A VR launch that was cut short leaves its temporary graphics settings in place: put the player's back first.
$journal = Join-Path $DataDir 'profile-active.json'
if (Test-Path -LiteralPath $journal) {
    # A file already equal to its original is skipped: a Documents file Windows would not let us write (Controlled folder
    # access, permissions) was never changed, and writing it again here stopped the reinstall (WRITEDENIED).
    $prof = Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
    $left = @()
    foreach ($file in $prof.files) {
        if ((Test-Path -LiteralPath $file.path) -and (Get-FileHash -LiteralPath $file.path).Hash -eq $file.sha256) { continue }
        if ((Test-Path -LiteralPath $file.backup) -and (Get-FileHash -LiteralPath $file.backup).Hash -eq $file.sha256) {
            try { [IO.File]::WriteAllBytes($file.path, [IO.File]::ReadAllBytes($file.backup)) }
            catch { $left += $file.path; Say "  Windows did not allow $($file.path) to be put back. Your original is saved at $($file.backup)." Yellow }
        }
    }
    if ($left.Count) { Say '  The VR launcher will try again and tell you what to do.' Yellow }
    else {
        Remove-Item -LiteralPath $journal -Force
        Say '  Settings from an interrupted VR launch were put back first.' Yellow
    }
}

# ---- 2. back up what we change (first install only, so the uninstaller always gets the player's originals) --------
Step '2. Backing up'
$bk = Join-Path $GameRoot $Mod.Backup
$first = -not (Test-Path (Join-Path $bk 'config\BmEngine.ini'))
if ($first) {
    New-Item -ItemType Directory -Force (Join-Path $bk 'config'), (Join-Path $bk 'documents'), (Join-Path $bk 'binaries') | Out-Null
    Copy-Item -LiteralPath $Engine (Join-Path $bk 'config\BmEngine.ini')
    Copy-Item -LiteralPath $Gfx (Join-Path $bk ('documents\' + $Mod.Gfx))
    [IO.File]::WriteAllText((Join-Path $bk 'documents_dir.txt'), $DocsDir, (New-Object Text.UTF8Encoding($false)))
    Say "  Your game settings were saved in $bk" Green
} else {
    Say '  Already backed up by an earlier install (kept as it is: those are your originals).'
}
# Someone else's xinput1_3.dll in the game folder (another mod): keep it, the uninstaller puts it back.
$px = Join-Path $GameDir 'xinput1_3.dll'
if ((Test-Path $px) -and -not ([IO.File]::ReadAllText($px, [Text.Encoding]::GetEncoding(28591)).Contains($Mod.ProxyMarker))) {
    if (-not (Test-Path (Join-Path $bk 'binaries\xinput1_3.dll'))) { Copy-Item $px (Join-Path $bk 'binaries\xinput1_3.dll') }
    Say '  Another xinput1_3.dll was in the game folder (another mod?). It is saved and comes back on uninstall.' Yellow
}

# ---- 3. the mod ---------------------------------------------------------------------------------
Step '3. Copying the mod into the game folder'
$files = @('xinput1_3.dll', 'batman_origins.dll', 'bo_xr_config32.exe', 'Batman Origins VR.bat', 'Batman Origins VR (SteamVR).bat',
           'Uninstall-BatmanOriginsVR.bat', 'Uninstall-BatmanOriginsVR.ps1')
foreach ($f in $files) {
    $src = Join-Path $Here $f
    if (-not (Test-Path $src)) { Fail "$f is missing from the download. Unpack the whole zip and run the installer from there." }
    Copy-Item $src (Join-Path $GameDir $f) -Force
}
foreach ($d in 'OriginsVR', 'BatmanOriginsDLSS') {
    $src = Join-Path $Here "files\$d"
    if (-not (Test-Path $src)) { Fail "files\$d is missing from the download. Unpack the whole zip and run the installer from there." }
    Copy-Item $src $GameDir -Recurse -Force
}
# Left over from development builds: their own uninstaller (refuses the new files) and file list.
foreach ($old in 'OriginsVR\Uninstall-VR.ps1') { $p = Join-Path $GameDir $old; if (Test-Path $p) { Remove-Item $p -Force } }
# The launcher checks the installed files against this list before every VR start.
$records = @()
foreach ($f in @('xinput1_3.dll', 'batman_origins.dll', 'bo_xr_config32.exe') +
              @(Get-ChildItem (Join-Path $GameDir 'OriginsVR'), (Join-Path $GameDir 'BatmanOriginsDLSS') -File |
                ForEach-Object { (Split-Path $_.DirectoryName -Leaf) + '/' + $_.Name })) {
    $p = Join-Path $GameDir $f
    $records += @{ name = $f; sha256 = (Get-FileHash -LiteralPath $p).Hash; bytes = (Get-Item -LiteralPath $p).Length }
}
$manifest = @{ version = (Get-Content (Join-Path $Here 'files\version.txt') -Raw).Trim(); game = $GameDir;
               installed = (Get-Date -Format o); files = $records } | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText((Join-Path $GameDir 'originsvr_manifest.json'), $manifest, (New-Object Text.UTF8Encoding($false)))
Say '  Mod files copied.' Green

# ---- 4. the mod's tested settings (first install only: never over the player's own) --------------------------
New-Item -ItemType Directory -Force $DataDir | Out-Null
if (-not (Test-Path (Join-Path $DataDir 'settings.ini'))) {
    Copy-Item (Join-Path $Here 'files\settings.ini') (Join-Path $DataDir 'settings.ini')
    Say '  Tested VR settings copied (change them later in the in-game VR menu).' Green
} else {
    Say '  Your VR settings from an earlier install are kept.'
}

# ---- 5. the game's settings for VR -----------------------------------------------------------------
# Both files are UTF-16 (the engine ini with a byte-order mark, NVIDIA's graphics store without): Graphics-Profile.ps1
# edits them byte-preserving. Its DoF switch also keeps a dated copy in the data folder.
Step '4. Setting the game up for VR'
. (Join-Path $Here 'files\OriginsVR\Graphics-Profile.ps1')
# WRITEDENIED: Windows may refuse changes to the Documents graphics store (Controlled folder access): leave it, say so.
$why = Test-OriginsWritable $Engine
if ($why) { Fail "Windows does not allow changes to the game's settings file $Engine ($why). To fix it, $OriginsWriteHelp, then run the installer again." }
$why = Test-OriginsWritable $Gfx
$GfxVR = $Gfx
if ($why) {
    Say "  Windows does not allow changes to $Gfx ($why)." Yellow
    Say "  It is left as it is: turn depth of field, motion blur and V-sync off in the game's own menu, or $OriginsWriteHelp and run the installer again." Yellow
    $GfxVR = $null
}
Disable-OriginsDepthOfField $GfxVR $Engine $DataDir
if ($GfxVR) {
    $gt = [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($Gfx))
    $gt2 = Set-OriginsOption (Set-OriginsOption $gt 'VSync' 'false') 'MotionBlur' 'false'
    if ($gt2 -cne $gt) { [IO.File]::WriteAllBytes($Gfx, [Text.Encoding]::Unicode.GetBytes($gt2)) }
}
$et = [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($Engine))
$et2 = $et
foreach ($kv in @(@('UseVsync', 'False'), @('MotionBlur', 'False'))) {
    $et2 = [regex]::Replace($et2, '(?mi)^(' + $kv[0] + '=)[^\r\n]*', ('${1}' + $kv[1]))
}
if ($et2 -cne $et) { [IO.File]::WriteAllBytes($Engine, [Text.Encoding]::Unicode.GetBytes($et2)) }
if ($GfxVR) { Say '  Depth of field, motion blur and V-sync off.' Green } else { Say '  The game folder''s depth of field, motion blur and V-sync settings are off.' Green }

# ---- 6. shortcuts -----------------------------------------------------------------------------------
if (-not $NoShortcuts) {
    Step '5. Making the shortcuts'
    $ws = New-Object -ComObject WScript.Shell
    $places = @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('Programs')) 'Batman Origins VR'))
    New-Item -ItemType Directory -Force $places[1] | Out-Null
    foreach ($p in $places) {
        foreach ($s in @(@('Batman Origins VR', 'Batman Origins VR.bat'), @('Batman Origins VR (SteamVR)', 'Batman Origins VR (SteamVR).bat'),
                         @('Uninstall Batman Origins VR', 'Uninstall-BatmanOriginsVR.bat'))) {
            if ($p -eq $places[0] -and $s[0] -like 'Uninstall*') { continue }   # the desktop gets the two launchers only
            $lnk = $ws.CreateShortcut((Join-Path $p ($s[0] + '.lnk')))
            $lnk.TargetPath = Join-Path $GameDir $s[1]
            $lnk.WorkingDirectory = $GameDir
            $lnk.IconLocation = (Join-Path $GameDir $Mod.Exe) + ',0'
            $lnk.Save()
        }
    }
    Say '  "Batman Origins VR" is on your desktop and in the Start menu.' Green
}

Write-Host ''
Write-Host '  Done. Start your PC VR software, then start the game with the "Batman Origins VR" shortcut.' -ForegroundColor White
Write-Host '  In the game, hold both thumbsticks down (or press F10) for the VR menu.'
Write-Host ''
