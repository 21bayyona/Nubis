@echo off
REM ===========================================================================
REM  NUBIS - build script for Windows 10/11 x64 (Visual Studio 2022 + CMake)
REM  Run from the project root:  scripts\build_windows.bat
REM ===========================================================================
setlocal
cd /d "%~dp0\.."

where cmake >nul 2>nul || (echo [ERROR] CMake not found. Install it from https://cmake.org/download/ & exit /b 1)
where git   >nul 2>nul || (echo [ERROR] Git not found. Install it from https://git-scm.com/download/win & exit /b 1)

echo.
echo === Configuring (first run downloads JUCE 8.0.4, ~150 MB) ===
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1

echo.
echo === Building Release ===
cmake --build build --config Release --parallel || exit /b 1

echo.
echo === Running DSP tests ===
build\Release\NubisDspTests.exe || echo [WARNING] DSP tests reported failures

echo.
echo Done. Plugins:
echo   build\Nubis_artefacts\Release\VST3\Nubis.vst3
echo   build\NubisInstrument_artefacts\Release\VST3\Nubis Instrument.vst3
echo Standalone test apps:
echo   build\Nubis_artefacts\Release\Standalone\Nubis.exe
echo.
echo Install with:  powershell -ExecutionPolicy Bypass -File scripts\install_windows.ps1
endlocal
