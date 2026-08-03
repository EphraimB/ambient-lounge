# PowerShell build & run script for Ambient Lounge Spatial TV Dashboard

$Compiler = "C:\Users\emb16\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260602-ucrt-x86_64\bin\g++.exe"

Write-Host "Compiling Native C++ Spatial TV Dashboard..." -ForegroundColor Cyan

& $Compiler -std=c++17 src/main.cpp -Ivendor -Ivendor/raylib/include -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -o ambient_lounge.exe

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Successful! Launching ambient_lounge.exe..." -ForegroundColor Green
    Start-Process -FilePath ".\ambient_lounge.exe"
} else {
    Write-Host "Build Failed." -ForegroundColor Red
}
