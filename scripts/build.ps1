# Builds PlaylistControl with CMake + Ninja + MSVC and runs the unit tests.
#
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Debug] [-SkipTests] [-VisualStudio]
#
# -VisualStudio generates build-vs\PlaylistControl.sln instead (Debug and Release).
param(
    [switch]$Debug,
    [switch]$SkipTests,
    [switch]$VisualStudio
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildType = if ($Debug) { "Debug" } else { "Release" }

if (-not (Test-Path (Join-Path $root "external\JUCE\CMakeLists.txt"))) {
    Write-Host "Fetching the JUCE submodule..."
    git -C $root submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed." }
}

# Visual Studio 2022 (any edition, or Build Tools) provides cl, rc, cmake and ninja.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = $null
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vs) { throw "Visual Studio 2022 with the C++ workload was not found." }
$vcvars = "$vs\VC\Auxiliary\Build\vcvars64.bat"
$tools = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake"
$env:PATH = "$tools\CMake\bin;$tools\Ninja;$env:PATH"

function Invoke-VS([string]$cmd) {
    cmd /c "call `"$vcvars`" >nul 2>&1 && $cmd"
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $cmd" }
}

Push-Location $root
try {
    if ($VisualStudio) {
        $buildDir = Join-Path $root "build-vs"
        Invoke-VS "cmake -G `"Visual Studio 17 2022`" -A x64 -S . -B `"$buildDir`""
        Invoke-VS "cmake --build `"$buildDir`" --config $buildType"
    } else {
        $buildDir = Join-Path $root ("build-" + $buildType.ToLower())
        Invoke-VS "cmake -G Ninja -S . -B `"$buildDir`" -DCMAKE_BUILD_TYPE=$buildType"
        Invoke-VS "cmake --build `"$buildDir`""
    }

    if (-not $SkipTests) {
        & "$buildDir\bin\playlistcontrol_tests.exe"
        if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
    }

    Write-Host ""
    Write-Host "PlaylistControl ($buildType): $buildDir\bin\PlaylistControl.exe"
} finally {
    Pop-Location
}
