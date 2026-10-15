[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('Configure', 'Build', 'Rebuild', 'Clean', 'Run')]
    [string]$Action = 'Build',
    [ValidateRange(1, 64)]
    [int]$Jobs = [Math]::Min(4, [Environment]::ProcessorCount)
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Keep Qt / compiler paths in sync with launch.json and c_cpp_properties.json.
$qtRoot = 'C:\Qt\Qt5.9.9\5.9.9\msvc2015_64'
$vcVarsAll = 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat'
$jom = 'C:\Qt\Qt5.9.9\Tools\QtCreator\bin\jom.exe'
$cmake = 'C:\CMake\bin\cmake.exe'
$projectRoot = Split-Path -Parent $PSScriptRoot
$runDir = Join-Path $projectRoot 'build\vscode-qt5.9.9-msvc2015_64'
$buildDir = Join-Path $runDir $Configuration
$program = Join-Path $buildDir 'bin\robot3ddemo.exe'
$qmake = Join-Path $qtRoot 'bin\qmake.exe'

function Invoke-Checked {
    param([string]$Tool, [string[]]$ToolArguments)
    & $Tool @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE." }
}

try {
    foreach ($required in @($qmake, $vcVarsAll)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Required tool not found: $required"
        }
    }
    # Third-party runtime directories must never enter the compiler's PATH.
    $environmentLines = & $env:ComSpec /d /s /c ('"' + $vcVarsAll + '" amd64 >nul && set')
    if ($LASTEXITCODE -ne 0) { throw 'vcvarsall.bat amd64 failed.' }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    $env:PATH = (Join-Path $qtRoot 'bin') + ';' + $env:PATH
    $env:VSLANG = '1033'
    $version = & $qmake -query QT_VERSION
    if ($LASTEXITCODE -ne 0 -or $version -ne '5.9.9') {
        throw "Expected Qt 5.9.9, got: $version"
    }

    if ($Action -eq 'Run') {
        if (-not (Test-Path -LiteralPath $program -PathType Leaf)) {
            throw "Executable not found: $program. Run the Build $Configuration task first."
        }
        $env:QT_PLUGIN_PATH = Join-Path $qtRoot 'plugins'
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $qtRoot 'plugins\platforms'
        # Qt 5.9 shader binary caches crash this machine's Intel OpenGL driver.
        $env:QT_DISABLE_SHADER_DISK_CACHE = '1'
        Push-Location $runDir
        try { Invoke-Checked $program @() } finally { Pop-Location }
        exit 0
    }

    foreach ($required in @($cmake, $jom)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Required tool not found: $required"
        }
    }
    if ($Action -eq 'Clean') {
        if (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt') -PathType Leaf) {
            Invoke-Checked $cmake @('--build', $buildDir, '--target', 'clean')
        } else { Write-Host "Nothing to clean for $Configuration." }
        exit 0
    }

    $configureArgs = @(
        '-S', $projectRoot, '-B', $buildDir, '-G', 'NMake Makefiles JOM',
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=$jom",
        "-DCMAKE_BUILD_TYPE:STRING=$Configuration",
        "-DROBOT3D_QT_ROOT:PATH=$qtRoot",
        "-DROBOT3D_RUNTIME_DIR:PATH=$runDir"
    )
    Write-Host "Configuring Qt $version / MSVC2015 x64 / $Configuration"
    Invoke-Checked $cmake $configureArgs
    if ($Action -eq 'Configure') { exit 0 }

    $buildArgs = @('--build', $buildDir, '--parallel', "$Jobs")
    if ($Action -eq 'Rebuild') { $buildArgs += '--clean-first' }
    Invoke-Checked $cmake $buildArgs
    if (-not (Test-Path -LiteralPath $program -PathType Leaf)) {
        throw "Build completed without the expected executable: $program"
    }
    Write-Host "Built: $program"
    Write-Host "Working directory: $runDir"
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
