<#
  Batman Origins VR uninstaller. The installer copies it into the game folder (next to BatmanOrigins.exe).
  Removes the mod, puts the game's settings back exactly as they were before the first install (BmEngine.ini and
  NVIDIA's GFXSettings file in Documents), removes the shortcuts and deletes the mod's own VR settings, logs and
  recordings (%LOCALAPPDATA%\Batman_Origins).
  Optional: -GameDir "<folder with BatmanOrigins.exe>" -Yes (no questions) -KeepSettings (keep the mod's VR settings,
  e.g. to reinstall) -DataDir "<mod data folder>" -KeepShortcuts -KeepSelf (practice runs)
#>
param([string]$GameDir, [string]$DataDir, [switch]$Yes, [switch]$KeepSettings, [switch]$KeepShortcuts, [switch]$KeepSelf)

$ErrorActionPreference = 'Stop'
function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Fail([string]$text) { Write-Host ''; Write-Host "  STOPPED: $text" -ForegroundColor Red; Write-Host ''; exit 1 }
$Here = [IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Path)
$Marker = '[batman_origins-proxy]'
$GfxName = 'GFXSettings.BatmanArkhamOrigins.xml'

Write-Host ''
Write-Host '  Batman Origins VR - uninstaller' -ForegroundColor White
Write-Host '  ------------------------------'
if (-not $GameDir) {
    if (Test-Path (Join-Path $Here 'BatmanOrigins.exe')) { $GameDir = $Here }
    else {
        try {
            $root = ((Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath) -replace '/', '\'
            $libs = @($root)
            $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
            if (Test-Path $vdf) { foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += ($m.Groups[1].Value -replace '\\\\', '\') } }
            foreach ($l in $libs) {
                $d = Join-Path $l 'steamapps\common\Batman Arkham Origins\SinglePlayer\Binaries\Win32'
                if (Test-Path (Join-Path $d 'BatmanOrigins.exe')) { $GameDir = $d; break }
            }
        } catch { }
    }
}
if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir 'BatmanOrigins.exe'))) { Fail 'the game was not found. Run this from the game folder (next to BatmanOrigins.exe).' }
if (Get-Process -Name 'BatmanOrigins' -ErrorAction SilentlyContinue) { Fail 'Batman: Arkham Origins is running. Close it first.' }
$GameDir = [IO.Path]::GetFullPath($GameDir)
$GameRoot = [IO.Path]::GetFullPath((Join-Path $GameDir '..\..\..'))
$Engine = Join-Path $GameRoot 'SinglePlayer\BMGame\Config\BmEngine.ini'
$bk = Join-Path $GameRoot 'BatmanOriginsVR_backup'
if (-not $DataDir) { $DataDir = Join-Path $env:LOCALAPPDATA 'Batman_Origins' }
Say "  Game: $GameDir"
if (-not $Yes) {
    $a = Read-Host '  Remove Batman Origins VR and put your game settings back? (y/n)'
    if ($a -notmatch '^[yY]') { Say '  Nothing changed.'; exit 0 }
}

# ---- a VR launch that was cut short: its temporary graphics settings go first ------------------------------
$journal = Join-Path $DataDir 'profile-active.json'
if (Test-Path -LiteralPath $journal) {
    # Skip files already equal to their original (a write Windows refused never changed them, WRITEDENIED). If one
    # really cannot be put back, stop BEFORE anything is removed.
    $prof = Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
    foreach ($file in $prof.files) {
        if ((Test-Path -LiteralPath $file.path) -and (Get-FileHash -LiteralPath $file.path).Hash -eq $file.sha256) { continue }
        if ((Test-Path -LiteralPath $file.backup) -and (Get-FileHash -LiteralPath $file.backup).Hash -eq $file.sha256) {
            try { [IO.File]::WriteAllBytes($file.path, [IO.File]::ReadAllBytes($file.backup)) }
            catch { Fail ("Windows did not allow your graphics settings file $($file.path) to be put back. Your original is " +
                          "saved at $($file.backup). Copy it back by hand, or allow PowerShell in Windows Security > Ransomware " +
                          'protection > Controlled folder access, then run the uninstaller again. Nothing was removed.') }
        }
    }
    Remove-Item -LiteralPath $journal -Force
}

# ---- the mod's files ------------------------------------------------------------------------------
$px = Join-Path $GameDir 'xinput1_3.dll'
if ((Test-Path $px) -and [IO.File]::ReadAllText($px, [Text.Encoding]::GetEncoding(28591)).Contains($Marker)) { Remove-Item $px -Force }
foreach ($f in 'batman_origins.dll', 'bo_xr_config32.exe', 'originsvr_manifest.json', 'Batman Origins VR.bat',
               'Batman Origins VR (SteamVR).bat') {
    $p = Join-Path $GameDir $f; if (Test-Path $p) { Remove-Item $p -Force }
}
foreach ($d in 'OriginsVR', 'BatmanOriginsDLSS') { $p = Join-Path $GameDir $d; if (Test-Path $p) { Remove-Item $p -Recurse -Force } }
if (Test-Path (Join-Path $bk 'binaries\xinput1_3.dll')) { Copy-Item (Join-Path $bk 'binaries\xinput1_3.dll') $px -Force; Say '  The xinput1_3.dll that was there before is back.' }
Say '  Mod files removed.' Green

# ---- the game's settings, exactly as they were before the mod ---------------------------------------
if (Test-Path (Join-Path $bk 'config\BmEngine.ini')) {
    Copy-Item (Join-Path $bk 'config\BmEngine.ini') $Engine -Force
    $docs = (Get-Content (Join-Path $bk 'documents_dir.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
    $g = Join-Path $bk "documents\$GfxName"
    if ($docs -and (Test-Path $g) -and (Test-Path $docs)) {
        # WRITEDENIED: if Windows refuses, stop here - the backup folder with the original is deleted only further down.
        $target = Join-Path $docs $GfxName
        if (-not (Test-Path $target) -or (Get-FileHash -LiteralPath $g).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
            try { Copy-Item $g $target -Force }
            catch { Fail ("The mod is removed, but Windows did not allow your graphics settings to be put back in $target. " +
                          "Your original is saved at $g. Copy it there by hand, or allow PowerShell in Windows Security > " +
                          'Ransomware protection > Controlled folder access, then run the uninstaller again.') }
        }
    }
    Say '  Your game settings are back as they were before the mod.' Green
} else {
    Say '  No settings backup found: the game settings were left as they are (depth of field, motion blur and V-sync stay off).' Yellow
}

# ---- shortcuts ----------------------------------------------------------------------------------------
if (-not $KeepShortcuts) {
    $desk = [Environment]::GetFolderPath('Desktop')
    foreach ($n in 'Batman Origins VR.lnk', 'Batman Origins VR (SteamVR).lnk') { $p = Join-Path $desk $n; if (Test-Path $p) { Remove-Item $p -Force } }
    $start = Join-Path ([Environment]::GetFolderPath('Programs')) 'Batman Origins VR'
    if (Test-Path $start) { Remove-Item $start -Recurse -Force }
}

# ---- the mod's own VR settings, logs and recordings (settings.ini, launch backups, captures) ---------
if ($KeepSettings) {
    Say "  Your VR settings are kept in $DataDir (-KeepSettings)."
} elseif (Test-Path $DataDir) {
    Remove-Item $DataDir -Recurse -Force
    Say '  The mod''s VR settings, logs and recordings are deleted.' Green
}
if (Test-Path $bk) { Remove-Item $bk -Recurse -Force }

Write-Host ''
Write-Host '  Batman Origins VR is removed.' -ForegroundColor White
Write-Host ''
# Remove ourselves when run from the game folder (the .bat deletes itself after this script is gone).
if (-not $KeepSelf -and $Here -eq $GameDir) { Remove-Item $MyInvocation.MyCommand.Path -Force -ErrorAction SilentlyContinue }
