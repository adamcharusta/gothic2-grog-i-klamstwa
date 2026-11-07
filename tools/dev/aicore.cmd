@echo off
rem Local Windows build + tests of aicore with MSVC (Visual Studio 2022+ with C++ workload).
rem Usage: tools\dev\aicore.cmd [extra ctest args]
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "ROOT=%~dp0..\.."
cmake -S "%ROOT%\aicore" -B "%ROOT%\build\aicore-msvc" -G Ninja -DCMAKE_BUILD_TYPE=Debug >nul || exit /b 1
cmake --build "%ROOT%\build\aicore-msvc" || exit /b 1
ctest --test-dir "%ROOT%\build\aicore-msvc" --output-on-failure %*
