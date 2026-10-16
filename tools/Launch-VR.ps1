param([switch]$SteamVR)
$ErrorActionPreference='Stop'
$binaryDir=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $binaryDir 'BatmanOrigins.exe'
$gameRoot=[IO.Path]::GetFullPath((Join-Path $binaryDir '../../..'))
$dataDir=Join-Path $env:LOCALAPPDATA 'Batman_Origins'
$journal=Join-Path $dataDir 'profile-active.json'
$manifestPath=Join-Path $binaryDir 'originsvr_manifest.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Origins VR installation manifest is missing.' }
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach($item in $manifest.files) {
    $path=Join-Path $binaryDir $item.name
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $item.sha256) {
        throw "Installed file differs: $($item.name). Reinstall the matching build."
    }
}
if ((Get-FileHash -LiteralPath $exe).Hash -ne '1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2') { throw 'Unsupported Origins executable.' }
$games=@(Get-Process -ErrorAction SilentlyContinue | Where-Object ProcessName -Match '^(BatmanOrigins|BatmanAC|BatmanAK|ACUnity|sekiro|DarkSoulsIII|armoredcore6|starwarsbattlefrontii|BioShockInfinite|BioshockHD)$')
if($games.Count) { throw "Close the running game first: $($games.ProcessName -join ', ')." }
New-Item -ItemType Directory -Path $dataDir -Force | Out-Null

function Restore-Profile($profile) {
    foreach($file in $profile.files) {
        if((Get-FileHash -LiteralPath $file.backup).Hash -ne $file.sha256) { throw "Profile backup differs: $($file.backup)" }
        [IO.File]::WriteAllBytes($file.path,[IO.File]::ReadAllBytes($file.backup))
    }
}
# Recover an interrupted launcher before saving a new baseline.
if(Test-Path -LiteralPath $journal) {
    Restore-Profile (Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json)
    Remove-Item -LiteralPath $journal
}
. (Join-Path $PSScriptRoot 'Graphics-Profile.ps1')
$docs=[Environment]::GetFolderPath('MyDocuments')
$gfx=Join-Path $docs 'WB Games/Batman Arkham Origins/GFXSettings.BatmanArkhamOrigins.xml'
$engine=Join-Path $gameRoot 'SinglePlayer/BMGame/Config/BmEngine.ini'
Disable-OriginsDepthOfField $gfx $engine $dataDir
function Find-SteamVR32 {
    $steam=(Get-ItemProperty 'HKCU:/SOFTWARE/Valve/Steam' -ErrorAction SilentlyContinue).SteamPath
    $candidates=@()
    # City SVRACTIVE: first the SteamVR install SteamVR itself treats as active (a player with two SteamVR installs).
    try { $active=(Get-Content -Raw (Join-Path $env:LOCALAPPDATA 'openvr/openvrpaths.vrpath') | ConvertFrom-Json).runtime[0]
          if($active) { $candidates+=Join-Path $active 'steamxr_win32.json' } } catch { }
    $candidates+=Join-Path (Split-Path $gameRoot -Parent) 'SteamVR/steamxr_win32.json'
    if($steam) { $candidates+=Join-Path $steam 'steamapps/common/SteamVR/steamxr_win32.json' }
    $json=$candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if($json) { [IO.Path]::GetFullPath($json) }
}
if($SteamVR) {
    $json=Find-SteamVR32
    if(!$json) { throw 'SteamVR steamxr_win32.json was not found. Install/update SteamVR first.' }
    $env:XR_RUNTIME_JSON=$json
}
$recommendation=& (Join-Path $binaryDir 'bo_xr_config32.exe')
# Exit 4 = the PC's VR software has no 32-bit runtime (Pimax): use SteamVR's 32-bit one for this launch only.
if($LASTEXITCODE -eq 4 -and !$SteamVR) {
    $json=Find-SteamVR32
    if($json) {
        Write-Host 'Your VR software has no 32-bit OpenXR runtime - using SteamVR''s for this launch.'
        $env:XR_RUNTIME_JSON=$json
        $recommendation=& (Join-Path $binaryDir 'bo_xr_config32.exe')
    }
}
if($LASTEXITCODE -ne 0) { throw 'Headset resolution query failed. Connect the headset to the selected runtime and try again.' }
$size=$recommendation | ConvertFrom-Json
$settings=Join-Path $dataDir 'settings.ini'
$scale=1.0; $mode=0
if(Test-Path -LiteralPath $settings) {
    $text=Get-Content -LiteralPath $settings -Raw
    if($text -match '(?m)^resolution_scale=([\d.]+)') { $scale=[double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture) }
    if($text -match '(?m)^dlss=(\d+)') { $mode=[int]$Matches[1] }
}
if($scale -lt .6 -or $scale -gt 1.8 -or $mode -lt 0 -or $mode -gt 5) { throw 'Invalid VR resolution/DLSS settings.' }
$ratios=@(1.0,1.0,.77,.667,.58,.5)
$ow=[int]([Math]::Ceiling($size.width*$scale/2)*2)
$oh=[int]([Math]::Ceiling($size.height*$scale/2)*2)
$rw=[int]([Math]::Ceiling($ow*$ratios[$mode]/2)*2)
$rh=[int]([Math]::Ceiling($oh*$ratios[$mode]/2)*2)
if($rw -lt 512 -or $rh -lt 512 -or $ow -gt 8192 -or $oh -gt 8192) { throw 'Runtime recommendation exceeds supported render bounds.' }
$backup=Join-Path $dataDir ('profiles/'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$docs=[Environment]::GetFolderPath('MyDocuments')
$gfx=Join-Path $docs 'WB Games/Batman Arkham Origins/GFXSettings.BatmanArkhamOrigins.xml'
$engine=Join-Path $gameRoot 'SinglePlayer/BMGame/Config/BmEngine.ini'
$files=@()
foreach($path in @($gfx,$engine)) {
    if(!(Test-Path -LiteralPath $path)) { throw "Expected game settings absent: $path" }
    $dest=Join-Path $backup ([IO.Path]::GetFileName($path))
    [IO.File]::WriteAllBytes($dest,[IO.File]::ReadAllBytes($path))
    $files+=@{path=$path;backup=$dest;sha256=(Get-FileHash -LiteralPath $path).Hash}
}
$profile=@{files=$files;runtime=$size.runtime;output=@($ow,$oh);render=@($rw,$rh);dlss=$mode}
$profile | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $journal -Encoding UTF8
try {
    # Exact replacements preserve UTF-16, BOM state and all unrelated values.
    $gfxText=[Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($gfx))
    foreach($option in @(@('Fullscreen','false'),@('ResolutionX',"$rw"),@('ResolutionY',"$rh"),@('VSync','false'))) {
        $pattern='(<OPTION\s+Name="'+$option[0]+'"[^>]*\bValue=")[^"]*(")'
        if([regex]::Matches($gfxText,$pattern).Count -ne 1) { throw "Graphics option is ambiguous: $($option[0])" }
        $replacement=$option[1]
        $gfxText=[regex]::Replace($gfxText,$pattern,{param($m) $m.Groups[1].Value+$replacement+$m.Groups[2].Value})
    }
    if($mode -gt 0) { $gfxText=Set-OriginsOption $gfxText 'AntiAliasing' 'OFF' }
    [IO.File]::WriteAllBytes($gfx,[Text.Encoding]::Unicode.GetBytes($gfxText))
    # Let XR pace the application. DLSS supplies AA without another temporal pass.
    $engineText=[Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($engine))
    $engineText=[regex]::Replace($engineText,'(?m)^(bSmoothFrameRate=)[^\r\n]*','$1False')
    if($mode -gt 0) {
        foreach($option in @(@('bAllowTXAA','False'),@('MaxMultisamples','1'),@('PostProcessAAType','0'),@('MLAAMode','0'))) {
            $pattern='(?mi)^('+$option[0]+'=)[^\r\n]*'; $value=$option[1]
            $engineText=[regex]::Replace($engineText,$pattern,{param($m) $m.Groups[1].Value+$value})
        }
    }
    [IO.File]::WriteAllBytes($engine,[Text.Encoding]::Unicode.GetBytes($engineText))
    Write-Host "Origins VR: $($size.runtime), scene ${rw}x${rh}, eyes ${ow}x${oh}."
    Write-Host 'Native stereo starts automatically. F10 opens the panel; face buttons / Start / Back skip opening titles.'
    $launchArguments=@('-bovr','-windowed',"ResX=$rw","ResY=$rh","-BOOutputX=$ow","-BOOutputY=$oh","-BODLSS=$mode")
    $process=Start-Process -FilePath $exe -ArgumentList $launchArguments -WorkingDirectory $binaryDir -PassThru
    $process.WaitForExit()
    # SteamStub can hand off to another process. Restore only after all Origins processes exit.
    while(Get-Process BatmanOrigins -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1 }
} finally {
    if(!(Get-Process BatmanOrigins -ErrorAction SilentlyContinue)) {
        Restore-Profile $profile
        Remove-Item -LiteralPath $journal -ErrorAction SilentlyContinue
        Write-Host 'Original desktop graphics settings restored.'
    }
}
