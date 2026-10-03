# Updates this Russian package from the latest OpenTS build published on GitHub.
#
# The release the build carries (OpenTS-<tag>-Win32.zip) holds Game.exe and
# Language.dll. This script downloads it, replaces the engine in both games,
# keeps the build check in step with the new engine, and records the tag it
# installed. It is safe to run offline: it reports the failure and exits.

param(
    [switch]$Quiet,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 defaults to TLS 1.0, which GitHub refuses.
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repo = 'hvkeyn/OpenTS'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Say([string]$text) {
    if (-not $Quiet) { Write-Host $text }
}

# Keep the build check honest about the engine that was just installed. The
# launcher and the check are stored in code page 866; the file listing is UTF-8.
function Update-Checks([string]$Root, [long]$Size, [string]$Hash) {
    $cp866 = [System.Text.Encoding]::GetEncoding(866)

    $play = Join-Path $Root 'Play.cmd'
    if (Test-Path $play) {
        $text = [IO.File]::ReadAllText($play, $cp866)
        $match = [regex]::Match($text, 'ENGINE_SIZE=(\d+)')
        if ($match.Success) {
            $text = $text.Replace('ENGINE_SIZE=' + $match.Groups[1].Value, 'ENGINE_SIZE=' + [string]$Size)
            [IO.File]::WriteAllText($play, $text, $cp866)
        }
    }

    $check = Join-Path $Root 'ПРОВЕРКА.cmd'
    if (Test-Path $check) {
        $text = [IO.File]::ReadAllText($check, $cp866)
        $match = [regex]::Match($text, '"(\d{6,})"')
        if ($match.Success) {
            $text = $text.Replace('"' + $match.Groups[1].Value + '"', '"' + [string]$Size + '"')
            [IO.File]::WriteAllText($check, $text, $cp866)
        }
    }

    $list = Join-Path $Root 'СПИСОК ФАЙЛОВ.txt'
    if (Test-Path $list) {
        $text = [IO.File]::ReadAllText($list, [System.Text.Encoding]::UTF8)
        $lines = $text -split "`r`n"
        $replacement = '${1}' + [string]$Size + '${2}' + $Hash
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match 'Game\.exe') {
                $lines[$i] = [regex]::Replace($lines[$i], '(Game\.exe\s+)\d+(\s+\S+\s+md5\s+)[0-9a-f]{32}', $replacement)
            }
        }
        [IO.File]::WriteAllText($list, ($lines -join "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
    }
}

$versionFile = Join-Path $root 'VERSION.txt'
$stampFile = Join-Path $root '.lastupdatecheck'

# Throttle the automatic check so a launch does not wait on the network every time.
if (-not $Force -and (Test-Path $stampFile)) {
    $age = (Get-Date) - (Get-Item -LiteralPath $stampFile).LastWriteTime
    if ($age.TotalHours -lt 6) {
        Say 'Update check skipped: checked recently.'
        exit 0
    }
}

try {
    $headers = @{ 'User-Agent' = 'OpenTS-RU-updater' }
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers $headers -TimeoutSec 8
} catch {
    # Record the attempt anyway so an offline machine does not wait on the timeout
    # before every launch.
    Set-Content -LiteralPath $stampFile -Value ((Get-Date).ToString('s')) -Encoding ASCII
    Say 'Update check failed (no network or no release yet).'
    exit 0
}

Set-Content -LiteralPath $stampFile -Value ((Get-Date).ToString('s')) -Encoding ASCII

$tag = $release.tag_name
$installed = ''
if (Test-Path $versionFile) {
    $installed = (Get-Content -LiteralPath $versionFile -Raw).Trim()
}

if (($installed -eq $tag) -and (-not $Force)) {
    Say "Already up to date ($tag)."
    exit 0
}

$asset = @($release.assets) | Where-Object { $_.name -match 'Win32\.zip$' } | Select-Object -First 1
if ($null -eq $asset) {
    Say "Release $tag carries no Win32 build."
    exit 0
}

Say "Downloading $tag ..."

$tmp = Join-Path $env:TEMP ('opents-ru-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
$zip = Join-Path $tmp 'release.zip'

try {
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -TimeoutSec 300 -UseBasicParsing
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $tmp 'x') -Force
} catch {
    Say 'Download or unpack failed.'
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    exit 0
}

$newExe = Get-ChildItem -Path (Join-Path $tmp 'x') -Recurse -Filter 'Game.exe' | Select-Object -First 1
if ($null -eq $newExe) {
    Say 'The build carries no Game.exe.'
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    exit 0
}

$newHash = (Get-FileHash -Algorithm MD5 -LiteralPath $newExe.FullName).Hash.ToLower()
$newSize = (Get-Item -LiteralPath $newExe.FullName).Length
$newDll = Get-ChildItem -Path (Join-Path $tmp 'x') -Recurse -Filter 'Language.dll' | Select-Object -First 1

$games = @(
    (Join-Path $root 'TiberianSun'),
    (Join-Path $root 'TwistedInsurrection')
)

$changed = $false
foreach ($game in $games) {
    $exe = Join-Path $game 'Game.exe'
    if (-not (Test-Path $exe)) { continue }

    $oldHash = (Get-FileHash -Algorithm MD5 -LiteralPath $exe).Hash.ToLower()
    if ($oldHash -eq $newHash) { continue }

    Copy-Item -LiteralPath $exe -Destination ($exe + '.bak') -Force
    Copy-Item -LiteralPath $newExe.FullName -Destination $exe -Force

    if ($null -ne $newDll) {
        $dst = Join-Path $game 'Language.dll'
        if (Test-Path $dst) { Copy-Item -LiteralPath $dst -Destination ($dst + '.bak') -Force }
        Copy-Item -LiteralPath $newDll.FullName -Destination $dst -Force
    }
    $changed = $true
}

if (-not $changed) {
    Set-Content -LiteralPath $versionFile -Value $tag -Encoding ASCII
    Say "Engine is already current ($tag)."
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    exit 0
}

Update-Checks -Root $root -Size $newSize -Hash $newHash
Set-Content -LiteralPath $versionFile -Value $tag -Encoding ASCII
Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue

Say "Updated to $tag."
Say "Engine md5: $newHash"
Say 'Run the build check to confirm the build.'
exit 0
