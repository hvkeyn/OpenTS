@echo off

chcp 866 >nul

setlocal

echo.

echo    Обновление сборки C^&C: Tiberian Sun / Twisted Insurrection

echo    Источник: github.com/hvkeyn/OpenTS, последний релиз.

echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1" -Force

echo.

echo    Готово. Если что-то не так - запусти ПРОВЕРКА.cmd.

echo.

pause

