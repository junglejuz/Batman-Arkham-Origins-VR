$ErrorActionPreference='Stop'
$binaryDir=Split-Path $PSScriptRoot -Parent
if(Get-Process BatmanOrigins -ErrorAction SilentlyContinue) { throw 'Close Origins before uninstalling.' }
$path=Join-Path $binaryDir 'originsvr_manifest.json'
$manifest=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
$targets=@()
foreach($item in $manifest.files) {
    $target=[IO.Path]::GetFullPath((Join-Path $binaryDir $item.name))
    if(!$target.StartsWith($binaryDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escapes game directory.' }
    if(Test-Path -LiteralPath $target) {
        if((Get-FileHash -LiteralPath $target).Hash -ne $item.sha256) { throw "File changed since installation: $target" }
        $targets+=$target
    }
}
$journal=Join-Path $env:LOCALAPPDATA 'Batman_Origins/profile-active.json'
if(Test-Path -LiteralPath $journal) {
    $profile=Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
    foreach($file in $profile.files) {
        if((Get-FileHash -LiteralPath $file.backup).Hash -ne $file.sha256) { throw 'Graphics backup hash differs.' }
        [IO.File]::WriteAllBytes($file.path,[IO.File]::ReadAllBytes($file.backup))
    }
    Remove-Item -LiteralPath $journal
}
foreach($target in $targets) { Remove-Item -LiteralPath $target }
Remove-Item -LiteralPath $path
Write-Host 'Origins VR files removed. Logs, captures and settings retained.'
