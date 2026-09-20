@echo off
rem ============================================================================
rem  Robot 3D Viewer Module - Standalone Port Demo : one-click build script
rem
rem  NOTE: keep this file ASCII-only. cmd.exe decodes .bat using the system
rem        code page (GBK on Chinese Windows); UTF-8 non-ASCII text would be
rem        mis-decoded and can corrupt the script syntax.
rem
rem  Requirements (versions verified on this machine):
rem    Qt 5.9.9 MinGW 32-bit : C:\Qt\Qt5.9.9\5.9.9\mingw53_32
rem    MinGW 5.3.0 32-bit    : C:\Qt\Qt5.9.9\Tools\mingw530_32
rem  Adjust the two variables below if your paths differ.
rem ============================================================================
setlocal

set "QT_DIR=C:\Qt\Qt5.9.9\5.9.9\mingw53_32"
set "MINGW_DIR=C:\Qt\Qt5.9.9\Tools\mingw530_32"

set "DEMO_DIR=%~dp0"
set "BUILD_DIR=%DEMO_DIR%build"
set "RUN_DIR=%DEMO_DIR%run"

if not exist "%QT_DIR%\bin\qmake.exe" (
    echo [ERROR] qmake not found: "%QT_DIR%\bin\qmake.exe"
    echo         Please edit QT_DIR at the top of this script.
    exit /b 1
)
if not exist "%MINGW_DIR%\bin\mingw32-make.exe" (
    echo [ERROR] mingw32-make not found: "%MINGW_DIR%\bin\mingw32-make.exe"
    echo         Please edit MINGW_DIR at the top of this script.
    exit /b 1
)

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"

echo.
echo [1/5] qmake : generating Makefile ...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"
qmake.exe "%DEMO_DIR%demo.pro" -o Makefile
if errorlevel 1 (
    echo [ERROR] qmake failed
    popd
    exit /b 1
)

echo.
echo [2/5] compiling ...
mingw32-make.exe -j4
if errorlevel 1 (
    echo [ERROR] build failed
    popd
    exit /b 1
)
popd

echo.
echo [3/5] deploying Qt runtime ...
if not exist "%RUN_DIR%" mkdir "%RUN_DIR%"
windeployqt.exe --release --no-translations --no-opengl-sw "%RUN_DIR%\robot3ddemo.exe" >nul

echo.
echo [4/5] copying assimp and MinGW runtime ...
copy /y "%DEMO_DIR%bin\libassimp.dll" "%RUN_DIR%\" >nul
if exist "%MINGW_DIR%\bin\libstdc++-6.dll"    copy /y "%MINGW_DIR%\bin\libstdc++-6.dll"    "%RUN_DIR%\" >nul
if exist "%MINGW_DIR%\bin\libgcc_s_dw2-1.dll" copy /y "%MINGW_DIR%\bin\libgcc_s_dw2-1.dll" "%RUN_DIR%\" >nul

echo.
echo [5/5] deploying robot models ...
if exist "%RUN_DIR%\robotType" rmdir /s /q "%RUN_DIR%\robotType"
xcopy /e /i /y /q "%DEMO_DIR%robotType" "%RUN_DIR%\robotType" >nul

echo.
if exist "%RUN_DIR%\robot3ddemo.exe" (
    echo ============================================
    echo  BUILD OK
    echo  Executable : %RUN_DIR%\robot3ddemo.exe
    echo  Run with   : %DEMO_DIR%run_demo.bat
    echo ============================================
) else (
    echo [ERROR] robot3ddemo.exe was not produced
    exit /b 1
)

endlocal
