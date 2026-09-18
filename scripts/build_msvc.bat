@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

rem ========== 环境检查 ==========
if "%CMAKE_PREFIX_PATH%"=="" if "%QTDIR%"=="" (
  echo [ERROR] Set CMAKE_PREFIX_PATH or QTDIR to your Qt MSVC kit ^(e.g. ...\6.11.2\msvc2022_64^)
  exit /b 1
)
if not "%CMAKE_PREFIX_PATH%"=="" (
  set "QT_PREFIX=%CMAKE_PREFIX_PATH%"
) else (
  set "QT_PREFIX=%QTDIR%"
)

rem 可选：CMAKE、JOM、VCVARS；未设置则尝试 PATH / vswhere
if "%CMAKE%"=="" set "CMAKE=cmake"
if exist "%CMAKE%" goto :cmake_ok
where "%CMAKE%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] cmake not found. Set CMAKE=full\path\cmake.exe or add cmake to PATH.
  exit /b 1
)
:cmake_ok

if "%VCVARS%"=="" (
  for /f "usebackq delims=" %%i in (`where vswhere 2^>nul`) do set "VSWHERE=%%i"
  if not defined VSWHERE if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  )
  if defined VSWHERE (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
      set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)
if "%VCVARS%"=="" (
  echo [ERROR] Set VCVARS to vcvars64.bat path, or install VS with C++ tools.
  exit /b 1
)
if not exist "%VCVARS%" (
  echo [ERROR] VCVARS not found: %VCVARS%
  exit /b 1
)

call "%VCVARS%" || exit /b 1
set "PATH=%QT_PREFIX%\bin;%PATH%"
if defined HALCONROOT if defined HALCONARCH set "PATH=%HALCONROOT%\bin\%HALCONARCH%;%PATH%"

set "SRC=%CD%"
set "OUT=%SRC%\..\MeasureExpert_runtime"
set "BDBG=%OUT%\.cmake\Debug"
set "BREL=%OUT%\.cmake\Release"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%BDBG%" mkdir "%BDBG%"
if not exist "%BREL%" mkdir "%BREL%"

set "GEN=Ninja"
where ninja >nul 2>&1
if errorlevel 1 (
  set "GEN=NMake Makefiles"
  where jom >nul 2>&1
  if not errorlevel 1 set "GEN=NMake Makefiles JOM"
)

echo ===== Source: %SRC% =====
echo ===== Runtime: %OUT% =====
echo ===== Generator: %GEN% =====

echo.
echo ===== Build Debug -^> MeasureExpertD.exe =====
"%CMAKE%" -S "%SRC%" -B "%BDBG%" -G "%GEN%" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DME_RUNTIME_DIR="%OUT%"
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%BDBG%"
if errorlevel 1 exit /b 1

echo.
echo ===== Build Release -^> MeasureExpert.exe =====
"%CMAKE%" -S "%SRC%" -B "%BREL%" -G "%GEN%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DME_RUNTIME_DIR="%OUT%"
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%BREL%"
if errorlevel 1 exit /b 1

echo.
echo ===== Done =====
dir "%OUT%\MeasureExpert*.exe"
exit /b 0
