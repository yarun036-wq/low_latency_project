@echo off
setlocal

if not exist build (
  cmake -S . -B build
)

cmake --build build --config Release
endlocal
