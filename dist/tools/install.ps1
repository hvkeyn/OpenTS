# Installs the patch from patch\ into the game folder given (or found next to the patch).
#
# Windows PowerShell cannot rely on the console code page for names, so everything here
# works with .NET paths: the patch carries files with Russian names, and cmd.exe read them
# differently depending on the code page it happened to be in.

param(
    [string]$Target = ''
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Say([string]$text) { Write-Host $text }

Say ''
Say '   Патч сборки Tiberian Sun / Firestorm / Twisted Insurrection'
Say ''
Say '   Ставит движок из релиза v0.2.3 во все три игры, шрифты с кириллицей,'
Say '   списки кампаний и брифинги, файлы проверки и автообновление с GitHub.'
Say ''

if ([string]::IsNullOrWhiteSpace($Target)) {
    $up = Join-Path (Split-Path -Parent $root) 'TiberianSun\Game.exe'
    $here = Join-Path $root 'TiberianSun\Game.exe'
    if (Test-Path -LiteralPath $up) {
        $Target = Split-Path -Parent $root
    } elseif (Test-Path -LiteralPath $here) {
        $Target = $root
    }
}

if ([string]::IsNullOrWhiteSpace($Target)) {
    Say '   Не вижу, где лежит сборка.'
    Say ''
    Say '   Положи патч рядом с папкой сборки и запусти:'
    Say ''
    Say '       Установить_патч.cmd "D:\Games\TiberiumSun"'
    Say ''
    Say '   Либо распакуй патч внутрь папки сборки и запусти его оттуда.'
    Say ''
    exit 1
}

$Target = (Resolve-Path -LiteralPath $Target).Path
$engine = Join-Path $Target 'TiberianSun\Game.exe'
if (-not (Test-Path -LiteralPath $engine)) {
    Say "   В папке $Target нет TiberianSun\Game.exe - это не папка сборки."
    Say ''
    exit 1
}

Say "   Папка сборки: $Target"
Say ''
Say '   Копирую файлы...'

$source = Join-Path $root 'patch'
$copied = 0
Get-ChildItem -LiteralPath $source -Recurse -File | ForEach-Object {
    $relative = $_.FullName.Substring($source.Length).TrimStart('\')
    $destination = Join-Path $Target $relative
    $folder = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $folder)) {
        New-Item -ItemType Directory -Path $folder | Out-Null
    }
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
    $copied++
}
Say "   Скопировано файлов: $copied"

# Имена, под которыми эти файлы лежали в старой сборке, только путают: убираем.
#
# Только те, что отличаются от новых имён больше, чем регистром: на Windows
# ПРОВЕРКА.cmd и Проверка.cmd - один и тот же файл, и уборка "старого" имени
# убрала бы только что скопированное новое. Здесь старые имена отличаются
# пробелом вместо подчёркивания, их можно убирать смело.
$legacy = @(
    'СПИСОК ФАЙЛОВ.txt',
    'КАК ИГРАТЬ.txt',
    'TiberianSun\Game_before_ti_patch.exe'
)
foreach ($name in $legacy) {
    $path = Join-Path $Target $name
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
        Say "   Убран старый файл: $name"
    }
}

Say ''
Say '   Готово. Проверяю сборку:'
Say ''

$check = Join-Path $Target 'Проверка.cmd'
if (Test-Path -LiteralPath $check) {
    & cmd.exe /c "`"$check`""
    $result = $LASTEXITCODE
    Say ''
    if ($result -eq 0) {
        Say '   Проверка пройдена.'
    } else {
        Say '   Проверка нашла расхождения - смотри строки выше.'
    }
} else {
    Say '   Файл Проверка.cmd не найден, проверь сборку вручную.'
}

Say ''
Say '   Дальше запускай Play.cmd. Обновления (Обновить.cmd) берутся с GitHub:'
Say '   github.com/hvkeyn/OpenTS.'
Say ''
exit 0
