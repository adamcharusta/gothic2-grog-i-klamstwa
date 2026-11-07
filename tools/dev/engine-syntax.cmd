@echo off
rem Fast local syntax check of the mod's engine sources (engine\common\grog\*.cpp) with MSVC, without
rem building OpenGothic (no Vulkan SDK needed). The real build happens in CI (MinGW).
rem Usage: tools\dev\engine-syntax.cmd [file.cpp ...]
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "ROOT=%~dp0..\.."
set "E=%ROOT%\engine"
set "NJ=%ROOT%\build\aicore-msvc\_deps\nlohmann_json-src\include"
set FILES=%*
if "%FILES%"=="" set FILES="%E%\common\grog\*.cpp"
cl /nologo /Zs /std:c++20 /EHsc /utf-8 /DNOMINMAX /D_USE_MATH_DEFINES /DGROG_AI=1 /DTEMPEST_BUILD_VULKAN=0 ^
  /I"%E%\common" /I"%E%\game" /I"%E%\lib\Tempest\Engine\include" /I"%E%\lib\ZenKit\include" ^
  /I"%E%\lib\bullet3\src" /I"%E%\lib\dmusic\include" /I"%E%\lib\rapidjson\include" /I"%E%\lib\miniz" ^
  /I"%ROOT%\aicore\include" /I"%NJ%" %FILES%
