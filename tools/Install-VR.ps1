param([string]$GameDir='E:/Games/Steam/steamapps/common/Batman Arkham Origins')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$binaryDir=[IO.Path]::GetFullPath((Join-Path $GameDir 'SinglePlayer/Binaries/Win32'))
$exe=Join-Path $binaryDir 'BatmanOrigins.exe'
if(Get-Process BatmanOrigins -ErrorAction SilentlyContinue) { throw 'Close Origins before installing.' }
$dataDir=Join-Path $env:LOCALAPPDATA 'Batman_Origins'
$journal=Join-Path $dataDir 'profile-active.json'
if(Test-Path -LiteralPath $journal) { throw 'A launch graphics journal is still active; wait for the launcher to restore it before installing.' }
if((Get-FileHash -LiteralPath $exe).Hash -ne '1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2') { throw 'Unsupported executable.' }
$manifestPath=Join-Path $binaryDir 'originsvr_manifest.json'
$previous=$null
if(Test-Path -LiteralPath $manifestPath) { $previous=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json }
$sources=[ordered]@{
    'xinput1_3.dll'=(Join-Path $project 'build/Release/xinput1_3.dll')
    'batman_origins.dll'=(Join-Path $project 'build/Release/batman_origins.dll')
    'bo_xr_config32.exe'=(Join-Path $project 'build/Release/bo_xr_config32.exe')
    'BatmanOriginsDLSS/bo_dlss64.exe'=(Join-Path $project 'build64/Release/bo_dlss64.exe')
    'BatmanOriginsDLSS/nvngx_dlss.dll'=(Join-Path $project '../akvr_native/third_party/ngx/bin/nvngx_dlss.dll')
    'OriginsVR/Launch-VR.ps1'=(Join-Path $project 'tools/Launch-VR.ps1')
    'OriginsVR/bat-wipe.bin'=(Join-Path $project 'build/Release/OriginsVR/bat-wipe.bin')
    'OriginsVR/Graphics-Profile.ps1'=(Join-Path $project 'tools/Graphics-Profile.ps1')
    'OriginsVR/Uninstall-VR.ps1'=(Join-Path $project 'tools/Uninstall-VR.ps1')
    'OriginsVR/LICENSE-bioshock-vr.txt'=(Join-Path $project 'LICENSE-bioshock-vr.txt')
    'OriginsVR/LICENSE-luma-framework.md'=(Join-Path $project 'LICENSE-luma-framework.md')
    'OriginsVR/LICENSE-MinHook.txt'=(Join-Path $project 'third_party/minhook/LICENSE.txt')
    'OriginsVR/LICENSE-ImGui.txt'=(Join-Path $project 'third_party/imgui/LICENSE.txt')
    'OriginsVR/LICENSE-OpenXR.txt'=(Join-Path $project 'third_party/OpenXR-SDK/LICENSE')
    'Batman Origins VR.bat'=(Join-Path $project 'tools/Batman Origins VR.bat')
    'Batman Origins VR (SteamVR).bat'=(Join-Path $project 'tools/Batman Origins VR (SteamVR).bat')
}
foreach($name in $sources.Keys) {
    if(!(Test-Path -LiteralPath $sources[$name])) { throw "Build/source file missing: $($sources[$name])" }
    $dest=Join-Path $binaryDir $name
    if(Test-Path -LiteralPath $dest) {
        $owned=$previous.files | Where-Object name -EQ $name
        if(!$owned -or (Get-FileHash -LiteralPath $dest).Hash -ne $owned.sha256) { throw "Existing file is not an unchanged Origins VR file: $dest" }
    }
}
# Retain the first-install graphics snapshot before reinforcing the requested DoF-off baseline.
$baseline=Join-Path $project 'baselines/first-install'
if(!(Test-Path -LiteralPath $baseline)) {
    New-Item -ItemType Directory -Path $baseline -Force | Out-Null
    $docs=[Environment]::GetFolderPath('MyDocuments')
    $gfx=Join-Path $docs 'WB Games/Batman Arkham Origins/GFXSettings.BatmanArkhamOrigins.xml'
    $engine=Join-Path $GameDir 'SinglePlayer/BMGame/Config/BmEngine.ini'
    foreach($path in @($gfx,$engine)) { Copy-Item -LiteralPath $path -Destination $baseline }
}
$records=@()
foreach($name in $sources.Keys) {
    $dest=Join-Path $binaryDir $name
    New-Item -ItemType Directory -Path (Split-Path $dest -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $sources[$name] -Destination "$dest.bovr-new"
    Move-Item -LiteralPath "$dest.bovr-new" -Destination $dest -Force
    $hash=(Get-FileHash -LiteralPath $dest).Hash
    if($hash -ne (Get-FileHash -LiteralPath $sources[$name]).Hash) { throw "Copy verification failed: $name" }
    $records+=@{name=$name;sha256=$hash;bytes=(Get-Item -LiteralPath $dest).Length}
}
. (Join-Path $PSScriptRoot 'Graphics-Profile.ps1')
Disable-OriginsDepthOfField (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WB Games/Batman Arkham Origins/GFXSettings.BatmanArkhamOrigins.xml') (Join-Path $GameDir 'SinglePlayer/BMGame/Config/BmEngine.ini') $dataDir
@{version='0.1.0-O1H';game=$binaryDir;installed=(Get-Date -Format o);files=$records} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $project 're/evidence/installed-build.json') -Force
Write-Host "Installed and hash-verified Origins VR build in $binaryDir"
