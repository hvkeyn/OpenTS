@echo off
chcp 65001 >nul
title Установка патча сборки
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Target "%~1"
echo.
echo    Нажмите любую клавишу...
pause >nul
