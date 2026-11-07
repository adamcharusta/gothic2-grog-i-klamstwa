@echo off
rem Grog i klamstwa launcher.
rem Reads the Gothic 2 NotR folder from gothic_path.txt (written by the installer) if present,
rem copies the mod manifest into <Gothic>\System and starts the OpenGothic fork.
setlocal
set "HERE=%~dp0"
set "GOTHIC="
if exist "%HERE%gothic_path.txt" set /p GOTHIC=<"%HERE%gothic_path.txt"

if defined GOTHIC (
  if not exist "%GOTHIC%\System\GrogIKlamstwa.ini" (
    copy /Y "%HERE%system\GrogIKlamstwa.ini" "%GOTHIC%\System\" >nul 2>&1
  )
  if not exist "%GOTHIC%\System\GrogIKlamstwa.ini" goto :nomanifest
  "%HERE%bin\Gothic2Notr.exe" -g "%GOTHIC%" -game:GrogIKlamstwa.ini %*
  goto :eof
)

rem No gothic_path.txt: let OpenGothic autodetect the install.
"%HERE%bin\Gothic2Notr.exe" -game:GrogIKlamstwa.ini %*
if errorlevel 1 goto :nomanifest
goto :eof

:nomanifest
echo.
echo [PL] Nie znaleziono pliku moda w folderze Gothica. Skopiuj system\GrogIKlamstwa.ini do ^<Gothic II^>\System\
echo      albo wpisz sciezke do Gothica w pliku gothic_path.txt obok tego pliku.
echo [EN] Mod manifest not found in the Gothic folder. Copy system\GrogIKlamstwa.ini into ^<Gothic II^>\System\
echo      or put the Gothic folder path into gothic_path.txt next to this file.
pause
