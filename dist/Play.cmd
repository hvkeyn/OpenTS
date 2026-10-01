@echo off

rem ---------------------------------------------------------------------------

rem C&C: Tiberian Sun, Firestorm и Twisted Insurrection на движке OpenTS.

rem Одна точка входа, три игры: у каждой своя папка с данными, потому что

rem движок читает правила, графику и театры один раз при запуске.

rem ---------------------------------------------------------------------------

setlocal
set "ENGINE_SIZE=7392256"

rem ---------------------------------------------------------------------------
rem Проверка: рядом должен лежать именно наш движок. Если сборку скопировали
rem поверх старой установки игры или распаковали не целиком, русский текст
rem в меню и списках идёт мусором, а миссии падают.
rem ---------------------------------------------------------------------------
if not exist "%~dp0TiberianSun\Game.exe" goto nobuild
for %%A in ("%~dp0TiberianSun\Game.exe") do set "ENGINE=%%~zA"
if not "%ENGINE%"=="%ENGINE_SIZE%" goto wrongbuild

title C&C Tiberian Sun - выбор игры



:menu

cls

echo.

echo    C&C: Tiberian Sun, Firestorm и Twisted Insurrection

echo    русская сборка на движке OpenTS

echo.

echo    Что запускаем:

echo.

echo      1 - Tiberian Sun          (кампании ГСБ и НОД, обучение)

echo      2 - Firestorm             (кампании дополнения и новые миссии)

echo      3 - Twisted Insurrection  (отдельная игра на том же движке)

echo.

echo      0 - выход

echo.

set "choice="

set /p "choice=Выбор и Enter: "



if "%choice%"=="1" goto tiberiansun

if "%choice%"=="2" goto firestorm

if "%choice%"=="3" goto insurrection

if "%choice%"=="0" exit /b 0

:nobuild
cls
echo.
echo    Не найден TiberianSun\Game.exe.
echo    Значит сборка распакована не целиком.
echo    Распакуй архив целиком и запусти Play.cmd из его корня.
echo.
pause
exit /b 1

:wrongbuild
cls
echo.
echo    Рядом лежит Game.exe не из этой сборки (размер %ENGINE%, нужен %ENGINE_SIZE%).
echo    Обычно это значит, что файлы скопировали поверх другой установки игры.
echo    Тогда русский текст в меню и списках идёт мусором, а миссии падают.
echo.
echo    Запусти ПРОВЕРКА.cmd: он покажет, какие файлы не те.
echo.
pause
exit /b 1


goto menu



:tiberiansun

cd /d "%~dp0TiberianSun"

start "" Game.exe -DATADIR="%~dp0TiberianSun" -GAME=TS

exit /b 0



:firestorm

cd /d "%~dp0TiberianSun"

start "" Game.exe -DATADIR="%~dp0TiberianSun" -GAME=FS

exit /b 0



:insurrection

cd /d "%~dp0TwistedInsurrection"

start "" Game.exe -DATADIR="%~dp0TwistedInsurrection"

exit /b 0

