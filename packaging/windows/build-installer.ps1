param([string]$BuildDir="build",[string]$OutputDir="dist")
$ErrorActionPreference="Stop"
cmake -S . -B $BuildDir
cmake --build $BuildDir --config Release
cmake --install $BuildDir --config Release --prefix $OutputDir
Write-Host "DENT installed to $OutputDir"
