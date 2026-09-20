@echo off
rem ============================================================================
rem  Robot 3D Viewer Module Demo - launcher
rem  Must run from the run\ directory: models are loaded via the relative
rem  path "robotType/..." (see RobotGeometry::ChangeRobot3D).
rem  NOTE: ASCII-only on purpose, see build.bat header.
rem ============================================================================
cd /d "%~dp0"
set "PATH=%~dp0;%PATH%"

echo Working directory: %CD%
echo Starting robot3ddemo.exe ...
robot3ddemo.exe

if errorlevel 1 (
    echo.
    echo [ERROR] exit code %errorlevel%
    echo See demo_debug.log in this directory for details.
    pause
)
