@echo off
rem Generates the Visual Studio solution into Engine\build and opens it.
cd /d "%~dp0Engine"
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 || (pause & exit /b 1)
start "" "build\Engine.slnx"
