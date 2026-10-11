param([Parameter(Mandatory=$true)][string]$Command)
$ErrorActionPreference='Stop'
$dir=Join-Path $env:LOCALAPPDATA 'Batman_Origins'
if(!(Test-Path -LiteralPath $dir)) { throw 'Origins VR has not initialized yet.' }
$path=Join-Path $dir 'command.txt'
[IO.File]::WriteAllText("$path.new",$Command.Trim(),[Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath "$path.new" -Destination $path -Force
Write-Host "Sent: $Command"
