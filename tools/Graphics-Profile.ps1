# Generated Origins graphics files are UTF-16. Decode/re-encode without
# stripping their original BOM (GSA has none; engine INI has one).
function Set-OriginsOption([string]$Text,[string]$Name,[string]$Value) {
    $pattern='(<OPTION\s+Name="'+[regex]::Escape($Name)+'"[^>]*\bValue=")[^"]*(")'
    if([regex]::Matches($Text,$pattern).Count -ne 1) { throw "Graphics option is ambiguous: $Name" }
    return [regex]::Replace($Text,$pattern,{param($m) $m.Groups[1].Value+$Value+$m.Groups[2].Value})
}
function Disable-OriginsDepthOfField([string]$Gfx,[string]$Engine,[string]$DataDir) {
    $oldGfx=[IO.File]::ReadAllBytes($Gfx); $oldEngine=[IO.File]::ReadAllBytes($Engine)
    $gt=Set-OriginsOption ([Text.Encoding]::Unicode.GetString($oldGfx)) 'DepthOfField' 'OFF'
    $et=[Text.Encoding]::Unicode.GetString($oldEngine)
    foreach($key in @('DepthOfField','NVDOF','DisableHighQualityDOF')) {
        $pattern='(?mi)^('+[regex]::Escape($key)+'=)[^\r\n]*'
        if(![regex]::IsMatch($et,$pattern)) { throw "Required DoF engine option absent: $key" }
        $value=if($key -eq 'DisableHighQualityDOF') {'True'} else {'False'}
        $et=[regex]::Replace($et,$pattern,{param($m) $m.Groups[1].Value+$value})
    }
    if($gt -ceq [Text.Encoding]::Unicode.GetString($oldGfx) -and $et -ceq [Text.Encoding]::Unicode.GetString($oldEngine)) { return }
    $backup=Join-Path $DataDir ('settings-backups/DoF-off-'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $backup ([IO.Path]::GetFileName($Gfx))),$oldGfx)
    [IO.File]::WriteAllBytes((Join-Path $backup ([IO.Path]::GetFileName($Engine))),$oldEngine)
    try {
        [IO.File]::WriteAllBytes($Gfx,[Text.Encoding]::Unicode.GetBytes($gt))
        [IO.File]::WriteAllBytes($Engine,[Text.Encoding]::Unicode.GetBytes($et))
    } catch {
        [IO.File]::WriteAllBytes($Gfx,$oldGfx); [IO.File]::WriteAllBytes($Engine,$oldEngine); throw
    }
    @{reason='JJ requested all DoF off';backup=$backup;files=@(
        @{path=$Gfx;sha256=(Get-FileHash -LiteralPath $Gfx).Hash},
        @{path=$Engine;sha256=(Get-FileHash -LiteralPath $Engine).Hash})} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
    Write-Host "All saved DoF options disabled. Previous settings backed up in $backup"
}
