@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1

if exist "C:\Users\yarun\OneDrive\Desktop\codex_api_test\c++\low_latency_lob\build" rmdir /s /q "C:\Users\yarun\OneDrive\Desktop\codex_api_test\c++\low_latency_lob\build"

"C:\Program Files\CMake\bin\cmake.exe" ^
  -S "C:\Users\yarun\OneDrive\Desktop\codex_api_test\c++\low_latency_lob" ^
  -B "C:\Users\yarun\OneDrive\Desktop\codex_api_test\c++\low_latency_lob\build" ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="C:\Users\yarun\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe" ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_AR="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe"
if errorlevel 1 exit /b 1

"C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\yarun\OneDrive\Desktop\codex_api_test\c++\low_latency_lob\build"
if errorlevel 1 exit /b 1
