@echo off
REM One-shot build with no dependency on make. Compiles every source file in
REM one g++ invocation and drops the exe in build\.
REM
REM Usage:  build.bat          release
REM         build.bat debug    -O0 -g, console window attached
REM         build.bat run      build then launch

setlocal enabledelayedexpansion

set SRC=
for %%f in (src\*.cpp src\core\*.cpp src\platform\*.cpp src\render\*.cpp src\game\*.cpp) do (
    if exist "%%f" set SRC=!SRC! %%f
)

if not exist build mkdir build

REM -static* is required here: dynamically linked MinGW output gets flagged
REM by Windows Defender on this machine and refuses to run.
set LIBS=-static -static-libgcc -static-libstdc++ -lgdi32 -luser32

if /I "%~1"=="debug" (
    set FLAGS=-std=c++17 -Wall -Wextra -O0 -g -DWOLF_DEBUG
) else (
    set FLAGS=-std=c++17 -Wall -Wextra -O2 -DNDEBUG -mwindows
)

echo Compiling...
g++ %FLAGS% %SRC% -o build\wolf3d.exe %LIBS%
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo Built build\wolf3d.exe

if /I "%~1"=="run" build\wolf3d.exe
endlocal
