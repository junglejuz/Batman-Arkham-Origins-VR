$ErrorActionPreference='Stop'
# WRITEDENIED (Asylum player report 2026-10-20): a write Windows refuses, simulated by the Read-only attribute.
. (Join-Path $PSScriptRoot '../tools/Graphics-Profile.ps1')
$dir=Join-Path ([IO.Path]::GetTempPath()) ('origins-profile-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
$utf16=[Text.Encoding]::Unicode
$gfx=Join-Path $dir 'GFXSettings.BatmanArkhamOrigins.xml';$engine=Join-Path $dir 'BmEngine.ini'
$gfxText="<GSA><OPTION Name=`"DepthOfField`" Value=`"ON`"/><OPTION Name=`"VSync`" Value=`"true`"/></GSA>"
$engineText="[SystemSettings]`r`nDepthOfField=True`r`nNVDOF=True`r`nDisableHighQualityDOF=False`r`nUnrelated=keep`r`n"
function Reset-Files { foreach($p in @($gfx,$engine)) { if(Test-Path $p) {(Get-Item $p).IsReadOnly=$false} }
    [IO.File]::WriteAllBytes($gfx,$utf16.GetBytes($gfxText));[IO.File]::WriteAllBytes($engine,$utf16.GetPreamble()+$utf16.GetBytes($engineText)) }
Reset-Files
if(Test-OriginsWritable $gfx) {throw 'A writable file was reported unwritable.'}
(Get-Item $gfx).IsReadOnly=$true
if(!(Test-OriginsWritable $gfx)) {throw 'A read-only file was reported writable.'}
# The depth-of-field step without the Documents store changes only the engine file.
$gfxHash=(Get-FileHash $gfx).Hash
Disable-OriginsDepthOfField $null $engine $dir | Out-Null
$et=$utf16.GetString([IO.File]::ReadAllBytes($engine))
if(!$et.Contains('DepthOfField=False') -or !$et.Contains('NVDOF=False') -or !$et.Contains('DisableHighQualityDOF=True') -or !$et.Contains('Unrelated=keep')) {throw 'Engine DoF options not set.'}
if((Get-FileHash $gfx).Hash -ne $gfxHash) {throw 'The refused Documents store was changed.'}
# With both files writable both change.
Reset-Files
Disable-OriginsDepthOfField $gfx $engine $dir | Out-Null
if(!$utf16.GetString([IO.File]::ReadAllBytes($gfx)).Contains('Name="DepthOfField" Value="OFF"')) {throw 'Graphics store DoF not set.'}
# A refused write inside the step leaves both files as they were.
Reset-Files;$before=@((Get-FileHash $gfx).Hash,(Get-FileHash $engine).Hash);(Get-Item $engine).IsReadOnly=$true;$threw=$false
try {Disable-OriginsDepthOfField $gfx $engine $dir | Out-Null} catch {$threw=$true}
(Get-Item $engine).IsReadOnly=$false
if(!$threw -or (Get-FileHash $gfx).Hash -ne $before[0] -or (Get-FileHash $engine).Hash -ne $before[1]) {throw 'A refused DoF write was not rolled back.'}
# The reporter's stuck journal: a file that was never changed is skipped on restore, not rewritten.
Reset-Files;$gfxBackup="$gfx.backup";$engineBackup="$engine.backup"
Copy-Item $gfx $gfxBackup;Copy-Item $engine $engineBackup
$journal=@{files=@(@{path=$gfx;backup=$gfxBackup;sha256=(Get-FileHash $gfx).Hash},@{path=$engine;backup=$engineBackup;sha256=(Get-FileHash $engine).Hash})}
(Get-Item $gfx).IsReadOnly=$true
Restore-OriginsProfile $journal
# A changed file that cannot be written back: the others are still restored, then one error names it and its original.
(Get-Item $gfx).IsReadOnly=$false;[IO.File]::WriteAllBytes($gfx,$utf16.GetBytes('changed'));(Get-Item $gfx).IsReadOnly=$true
[IO.File]::WriteAllBytes($engine,$utf16.GetBytes('changed'));$failed=$null
try {Restore-OriginsProfile $journal} catch {$failed="$_"}
if(!$failed -or !$failed.Contains($gfx) -or !$failed.Contains($gfxBackup)) {throw 'A failed restore did not name the file and its backup.'}
if((Get-FileHash $engine).Hash -ne $journal.files[1].sha256) {throw 'One failed restore stopped the other file being restored.'}
(Get-Item $gfx).IsReadOnly=$false
Remove-Item -LiteralPath $dir -Recurse -Force
Write-Output 'PASS: write test, DoF step without the Documents store / with both / rolled back on refusal, unchanged unwritable file skipped on restore, partial restore names the failed file.'
