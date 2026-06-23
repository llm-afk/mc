@echo off
setlocal

REM ============================================================
REM ADM32F03X Auto Flash Launcher
REM Find Python 3: system install > portable > error
REM ============================================================

REM 1. Try system Python
where python >nul 2>&1
if %errorlevel% equ 0 (
    python --version 2>&1 | findstr /i "Python 3" >nul
    if %errorlevel% equ 0 (
        python "%~dp0auto_flash.py"
        goto :end
    )
)

REM 2. Try system python3
where python3 >nul 2>&1
if %errorlevel% equ 0 (
    python3 "%~dp0auto_flash.py"
    goto :end
)

REM 3. Try portable Python (python-embed folder)
if exist "%~dp0python-embed\python.exe" (
    "%~dp0python-embed\python.exe" "%~dp0auto_flash.py"
    goto :end
)

REM 4. No Python found
echo.
echo  ============================================================
echo   [ERROR] Python 3 not found
echo  ============================================================
echo.
echo   Please install Python 3:
echo.
echo   Option 1 (Recommended): Official installer
echo     https://www.python.org/downloads/
echo     * Check "Add Python to PATH" during install
echo.
echo   Option 2 (Portable): Extract Python Embeddable to
echo     "%~dp0python-embed\"
echo     https://www.python.org/downloads/windows/
echo     (Look for "Windows embeddable package (64-bit)")
echo.
pause
goto :eof

:end
echo.
echo  ============================================================
echo   Flash complete. Press any key to exit...
echo  ============================================================
pause >nul
endlocal