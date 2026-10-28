@echo off
setlocal
echo ============================================
echo   Building Smart Inventory GUI Application
echo ============================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo Error: Unable to locate or run MSVC vcvars64.bat
    exit /b 1
)

del /q SmartInventoryGUI.old.exe >nul 2>&1
if exist SmartInventoryGUI.exe (
    move /y SmartInventoryGUI.exe SmartInventoryGUI.old.exe >nul 2>&1
)

cl.exe /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS gui_main.c /link /out:SmartInventoryGUI.exe /subsystem:windows user32.lib gdi32.lib comctl32.lib shell32.lib uxtheme.lib
if errorlevel 1 (
    echo.
    echo Compilation Failed!
    if exist SmartInventoryGUI.old.exe move /y SmartInventoryGUI.old.exe SmartInventoryGUI.exe >nul 2>&1
    exit /b 1
)

del /q SmartInventoryGUI.old.exe >nul 2>&1
del /q gui_main.obj >nul 2>&1

echo.
echo ============================================
echo   Build Successful: SmartInventoryGUI.exe
echo ============================================
endlocal
