@echo off
chcp 65001 >nul
set "ROOT=%~dp0"
set "BAD=0"
echo.
echo    Проверка сборки: Tiberian Sun, Firestorm, Twisted Insurrection
echo    Папка: %ROOT%
echo.
if not exist "%ROOT%TiberianSun\Game.exe" echo    НЕТ    TiberianSun\Game.exe  -  движок
if not exist "%ROOT%TiberianSun\Game.exe" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\Game.exe") do if not "%%~zA"=="7394304" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\Game.exe") do if not "%%~zA"=="7394304" echo    НЕ ТО  TiberianSun\Game.exe  -  движок
if not exist "%ROOT%TwistedInsurrection\Game.exe" echo    НЕТ    TwistedInsurrection\Game.exe  -  движок TI
if not exist "%ROOT%TwistedInsurrection\Game.exe" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\Game.exe") do if not "%%~zA"=="7394304" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\Game.exe") do if not "%%~zA"=="7394304" echo    НЕ ТО  TwistedInsurrection\Game.exe  -  движок TI
if not exist "%ROOT%TiberianSun\Language.dll" echo    НЕТ    TiberianSun\Language.dll  -  русские строки
if not exist "%ROOT%TiberianSun\Language.dll" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\Language.dll") do if not "%%~zA"=="116736" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\Language.dll") do if not "%%~zA"=="116736" echo    НЕ ТО  TiberianSun\Language.dll  -  русские строки
if not exist "%ROOT%TwistedInsurrection\Language.dll" echo    НЕТ    TwistedInsurrection\Language.dll  -  русские строки TI
if not exist "%ROOT%TwistedInsurrection\Language.dll" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\Language.dll") do if not "%%~zA"=="116736" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\Language.dll") do if not "%%~zA"=="116736" echo    НЕ ТО  TwistedInsurrection\Language.dll  -  русские строки TI
if not exist "%ROOT%TiberianSun\8point.fnt" echo    НЕТ    TiberianSun\8point.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\8point.fnt" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\8point.fnt") do if not "%%~zA"=="12097" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\8point.fnt") do if not "%%~zA"=="12097" echo    НЕ ТО  TiberianSun\8point.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\12metfnt.fnt" echo    НЕТ    TiberianSun\12metfnt.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\12metfnt.fnt" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\12metfnt.fnt") do if not "%%~zA"=="15727" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\12metfnt.fnt") do if not "%%~zA"=="15727" echo    НЕ ТО  TiberianSun\12metfnt.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\6point.fnt" echo    НЕТ    TiberianSun\6point.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\6point.fnt" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\6point.fnt") do if not "%%~zA"=="12644" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\6point.fnt") do if not "%%~zA"=="12644" echo    НЕ ТО  TiberianSun\6point.fnt  -  шрифт с кириллицей
if not exist "%ROOT%TiberianSun\fullfnt3.shp" echo    НЕТ    TiberianSun\fullfnt3.shp  -  русские буквы брифингов
if not exist "%ROOT%TiberianSun\fullfnt3.shp" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\fullfnt3.shp") do if not "%%~zA"=="71209" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\fullfnt3.shp") do if not "%%~zA"=="71209" echo    НЕ ТО  TiberianSun\fullfnt3.shp  -  русские буквы брифингов
if not exist "%ROOT%TiberianSun\fullfnt3.pal" echo    НЕТ    TiberianSun\fullfnt3.pal  -  палитра букв
if not exist "%ROOT%TiberianSun\fullfnt3.pal" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\fullfnt3.pal") do if not "%%~zA"=="768" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\fullfnt3.pal") do if not "%%~zA"=="768" echo    НЕ ТО  TiberianSun\fullfnt3.pal  -  палитра букв
if not exist "%ROOT%TiberianSun\INI\Battle.ini" echo    НЕТ    TiberianSun\INI\Battle.ini  -  список кампаний
if not exist "%ROOT%TiberianSun\INI\Battle.ini" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\INI\Battle.ini") do if not "%%~zA"=="6711" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\INI\Battle.ini") do if not "%%~zA"=="6711" echo    НЕ ТО  TiberianSun\INI\Battle.ini  -  список кампаний
if not exist "%ROOT%TiberianSun\INI\LANGFS.INI" echo    НЕТ    TiberianSun\INI\LANGFS.INI  -  строки Firestorm
if not exist "%ROOT%TiberianSun\INI\LANGFS.INI" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\INI\LANGFS.INI") do if not "%%~zA"=="20528" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\INI\LANGFS.INI") do if not "%%~zA"=="20528" echo    НЕ ТО  TiberianSun\INI\LANGFS.INI  -  строки Firestorm
if not exist "%ROOT%TiberianSun\MISSION1.INI" echo    НЕТ    TiberianSun\MISSION1.INI  -  брифинги миссий
if not exist "%ROOT%TiberianSun\MISSION1.INI" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\MISSION1.INI") do if not "%%~zA"=="25883" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\MISSION1.INI") do if not "%%~zA"=="25883" echo    НЕ ТО  TiberianSun\MISSION1.INI  -  брифинги миссий
if not exist "%ROOT%TiberianSun\MISSION.INI" echo    НЕТ    TiberianSun\MISSION.INI  -  брифинги кампаний
if not exist "%ROOT%TiberianSun\MISSION.INI" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\MISSION.INI") do if not "%%~zA"=="215814" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\MISSION.INI") do if not "%%~zA"=="215814" echo    НЕ ТО  TiberianSun\MISSION.INI  -  брифинги кампаний
if not exist "%ROOT%TiberianSun\SUPERPOWERS.INI" echo    НЕТ    TiberianSun\SUPERPOWERS.INI  -  панель суперсил
if not exist "%ROOT%TiberianSun\SUPERPOWERS.INI" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\SUPERPOWERS.INI") do if not "%%~zA"=="8949" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\SUPERPOWERS.INI") do if not "%%~zA"=="8949" echo    НЕ ТО  TiberianSun\SUPERPOWERS.INI  -  панель суперсил
if not exist "%ROOT%TiberianSun\SUN.INI" echo    НЕТ    TiberianSun\SUN.INI  -  разрешение и скорость
if not exist "%ROOT%TiberianSun\SUN.INI" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\SUN.INI") do if not "%%~zA"=="914" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\SUN.INI") do if not "%%~zA"=="914" echo    НЕ ТО  TiberianSun\SUN.INI  -  разрешение и скорость
if not exist "%ROOT%TiberianSun\TEST01.MAP" echo    НЕТ    TiberianSun\TEST01.MAP  -  полигон 1
if not exist "%ROOT%TiberianSun\TEST01.MAP" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST01.MAP") do if not "%%~zA"=="105040" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST01.MAP") do if not "%%~zA"=="105040" echo    НЕ ТО  TiberianSun\TEST01.MAP  -  полигон 1
if not exist "%ROOT%TiberianSun\TEST02.MAP" echo    НЕТ    TiberianSun\TEST02.MAP  -  полигон 2
if not exist "%ROOT%TiberianSun\TEST02.MAP" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST02.MAP") do if not "%%~zA"=="106270" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST02.MAP") do if not "%%~zA"=="106270" echo    НЕ ТО  TiberianSun\TEST02.MAP  -  полигон 2
if not exist "%ROOT%TiberianSun\TEST03.MAP" echo    НЕТ    TiberianSun\TEST03.MAP  -  полигон 3
if not exist "%ROOT%TiberianSun\TEST03.MAP" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST03.MAP") do if not "%%~zA"=="105932" set "BAD=1"
for %%A in ("%ROOT%TiberianSun\TEST03.MAP") do if not "%%~zA"=="105932" echo    НЕ ТО  TiberianSun\TEST03.MAP  -  полигон 3
if not exist "%ROOT%TwistedInsurrection\INI\Battle.ini" echo    НЕТ    TwistedInsurrection\INI\Battle.ini  -  список кампаний TI
if not exist "%ROOT%TwistedInsurrection\INI\Battle.ini" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\INI\Battle.ini") do if not "%%~zA"=="7396" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\INI\Battle.ini") do if not "%%~zA"=="7396" echo    НЕ ТО  TwistedInsurrection\INI\Battle.ini  -  список кампаний TI
if not exist "%ROOT%TwistedInsurrection\SUPERPOWERS.INI" echo    НЕТ    TwistedInsurrection\SUPERPOWERS.INI  -  панель суперсил TI
if not exist "%ROOT%TwistedInsurrection\SUPERPOWERS.INI" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\SUPERPOWERS.INI") do if not "%%~zA"=="8949" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\SUPERPOWERS.INI") do if not "%%~zA"=="8949" echo    НЕ ТО  TwistedInsurrection\SUPERPOWERS.INI  -  панель суперсил TI
if not exist "%ROOT%TwistedInsurrection\8point.fnt" echo    НЕТ    TwistedInsurrection\8point.fnt  -  шрифт с кириллицей TI
if not exist "%ROOT%TwistedInsurrection\8point.fnt" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\8point.fnt") do if not "%%~zA"=="12097" set "BAD=1"
for %%A in ("%ROOT%TwistedInsurrection\8point.fnt") do if not "%%~zA"=="12097" echo    НЕ ТО  TwistedInsurrection\8point.fnt  -  шрифт с кириллицей TI
echo.
if "%BAD%"=="1" goto bad
echo    Всё на месте: движок, шрифты с кириллицей, списки кампаний, полигоны.
echo.
echo    Контрольный хеш движка TiberianSun\Game.exe:
powershell -NoProfile -Command "(Get-FileHash -Algorithm MD5 -LiteralPath '%ROOT%TiberianSun\Game.exe').Hash.ToLower()"
echo.
echo    Сверь его со значением в Список_файлов.txt - тогда сборка точно та.
echo.
echo    Нажмите любую клавишу...
pause >nul
exit /b 0

:bad
echo.
echo    ЕСТЬ ПРОБЛЕМЫ: каких-то файлов нет или они от старой версии.
echo    Распакуй архив заново, целиком, в НОВУЮ пустую папку.
echo    Не копируй файлы поверх другой установки игры: рядом окажется
echo    старый Game.exe или старые шрифты, и тогда русский текст в меню
echo    идёт мусором, а миссии падают.
echo.
echo    Нажмите любую клавишу...
pause >nul
exit /b 1
