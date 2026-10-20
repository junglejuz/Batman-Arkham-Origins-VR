# Generated Origins graphics files are UTF-16. Decode/re-encode without
# stripping their original BOM (GSA has none; engine INI has one).
function Set-OriginsOption([string]$Text,[string]$Name,[string]$Value) {
    $pattern='(<OPTION\s+Name="'+[regex]::Escape($Name)+'"[^>]*\bValue=")[^"]*(")'
    if([regex]::Matches($Text,$pattern).Count -ne 1) { throw "Graphics option is ambiguous: $Name" }
    return [regex]::Replace($Text,$pattern,{param($m) $m.Groups[1].Value+$Value+$m.Groups[2].Value})
}
function Test-OriginsWritable([string]$Path) {
    # Opens for writing without changing a byte; returns why Windows refused, or nothing. Catches the Read-only
    # attribute, NTFS permissions and Windows Security's Controlled folder access (blocks PowerShell in Documents) -
    # Asylum player report 2026-10-20 (WRITEDENIED); Origins' graphics store lives in Documents too.
    try { [IO.File]::Open($Path,'Open','Write','ReadWrite').Dispose() }
    catch { return $_.Exception.GetBaseException().Message }
}
$OriginsWriteHelp='untick Read-only in its Properties, or allow PowerShell in Windows Security > Virus & threat protection > Ransomware protection > Controlled folder access'
function Restore-OriginsProfile($Journal) {
    # A file already equal to its original is left alone: a write Windows refused never changed it, and writing it again
    # here would fail, keep the journal and stop every later launch, install and uninstall (WRITEDENIED). One file that
    # cannot be put back does not stop the others; the error at the end names each one and where its original is.
    $failed=@()
    foreach($file in $Journal.files) {
        if((Test-Path -LiteralPath $file.path) -and (Get-FileHash -LiteralPath $file.path).Hash -eq $file.sha256) { continue }
        if((Get-FileHash -LiteralPath $file.backup).Hash -ne $file.sha256) { throw "Profile backup differs: $($file.backup)" }
        try { [IO.File]::WriteAllBytes($file.path,[IO.File]::ReadAllBytes($file.backup)) }
        catch { $failed+="$($file.path) ($($_.Exception.GetBaseException().Message)); your original is saved at $($file.backup)" }
    }
    if($failed.Count) { throw "Windows did not allow your original graphics settings to be put back: $($failed -join '; '). Copy each original back by hand, or $OriginsWriteHelp, then start VR again." }
}
# $Gfx may be empty: the Documents graphics store is then left as it is (Windows refused writing it, WRITEDENIED).
function Disable-OriginsDepthOfField([string]$Gfx,[string]$Engine,[string]$DataDir) {
    $targets=@($Engine); if($Gfx) { $targets=@($Gfx,$Engine) }
    $old=@{}; foreach($path in $targets) { $old[$path]=[IO.File]::ReadAllBytes($path) }
    $new=@{}
    if($Gfx) { $new[$Gfx]=Set-OriginsOption ([Text.Encoding]::Unicode.GetString($old[$Gfx])) 'DepthOfField' 'OFF' }
    $et=[Text.Encoding]::Unicode.GetString($old[$Engine])
    foreach($key in @('DepthOfField','NVDOF','DisableHighQualityDOF')) {
        $pattern='(?mi)^('+[regex]::Escape($key)+'=)[^\r\n]*'
        if(![regex]::IsMatch($et,$pattern)) { throw "Required DoF engine option absent: $key" }
        $value=if($key -eq 'DisableHighQualityDOF') {'True'} else {'False'}
        $et=[regex]::Replace($et,$pattern,{param($m) $m.Groups[1].Value+$value})
    }
    $new[$Engine]=$et
    $changed=@($targets | Where-Object { $new[$_] -cne [Text.Encoding]::Unicode.GetString($old[$_]) })
    if(!$changed.Count) { return }
    $backup=Join-Path $DataDir ('settings-backups/DoF-off-'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    foreach($path in $targets) { [IO.File]::WriteAllBytes((Join-Path $backup ([IO.Path]::GetFileName($path))),$old[$path]) }
    try { foreach($path in $changed) { [IO.File]::WriteAllBytes($path,[Text.Encoding]::Unicode.GetBytes($new[$path])) } }
    catch {
        # Put back whatever was written; a file Windows refused was never changed.
        foreach($path in $changed) { try { [IO.File]::WriteAllBytes($path,$old[$path]) } catch { } }
        throw
    }
    @{reason='JJ requested all DoF off';backup=$backup;files=@($targets | ForEach-Object { @{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash} })} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
    Write-Host "All saved DoF options disabled. Previous settings backed up in $backup"
}
