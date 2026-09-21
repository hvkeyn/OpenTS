@echo off
rem ---------------------------------------------------------------------------
rem C&C: Tiberian Sun, Firestorm и Twisted Insurrection на движке OpenTS.
rem Одна точка входа, три игры: у каждой своя папка с данными, потому что
rem движок читает правила, графику и театры один раз при запуске.
rem ---------------------------------------------------------------------------
setlocal
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
