[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('Configure', 'Build', 'Rebuild', 'Clean', 'Run')]
    [string]$Action = 'Build',
    # Retain the legacy argument for existing command lines; VTK is enabled by default.
    [switch]$WithVTK,
    [string]$VtkDir = '',

    [string]$OcctDir = '',

    [switch]$WithoutOCCT,

    [switch]$WithoutVTK,
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
$runName = if ($WithoutVTK) { 'build\vscode-qt5.9.9-msvc2015_64' } else { 'build\vscode-qt5.9.9-msvc2015_64-vtk' }
$runDir = Join-Path $projectRoot $runName
$buildDir = Join-Path $runDir $Configuration
$program = Join-Path $buildDir 'bin\robot3ddemo.exe'
$qmake = Join-Path $qtRoot 'bin\qmake.exe'

function Invoke-Checked
{
    param([string]$Tool, [string[]]$ToolArguments)
    & $Tool @ToolArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Tool failed with exit code $LASTEXITCODE."
    }
}

try
{
    foreach ($required in @($qmake, $vcVarsAll))
    {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf))
        {
            throw "Required tool not found: $required"
        }
    }
    # Third-party runtime directories must never enter the compiler's PATH.
    $environmentLines = & $env:ComSpec /d /s /c ('"' + $vcVarsAll + '" amd64 >nul && set')
    if ($LASTEXITCODE -ne 0)
    {
        throw 'vcvarsall.bat amd64 failed.'
    }
    foreach ($line in $environmentLines)
    {
        if ($line -match '^([^=]+)=(.*)$')
        {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    $env:PATH = (Join-Path $qtRoot 'bin') + ';' + $env:PATH
    $env:VSLANG = '1033'
    $version = & $qmake -query QT_VERSION
    if ($LASTEXITCODE -ne 0 -or $version -ne '5.9.9')
    {
        throw "Expected Qt 5.9.9, got: $version"
    }

    if ($Action -eq 'Run')
    {
        if (-not (Test-Path -LiteralPath $program -PathType Leaf))
        {
            throw "Executable not found: $program. Run the Build $Configuration task first."
        }
        $env:QT_PLUGIN_PATH = Join-Path $qtRoot 'plugins'
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $qtRoot 'plugins\platforms'
        # Qt 5.9 shader binary caches crash this machine's Intel OpenGL driver.
        $env:QT_DISABLE_SHADER_DISK_CACHE = '1'
        $env:DEMO_RENDERER = if ($WithoutVTK) { 'opengl' } else { 'vtk' }
        Push-Location $runDir
        try
        {
            Invoke-Checked $program @()
        }
        finally
        {
            Pop-Location
        }
        exit 0
    }

    foreach ($required in @($cmake, $jom))
    {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf))
        {
            throw "Required tool not found: $required"
        }
    }

    if ($Action -eq 'Clean')
    {
        if (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt') -PathType Leaf)
        {
            Invoke-Checked $cmake @('--build', $buildDir, '--target', 'clean')
        }
        else
        {
            Write-Host "Nothing to clean for $Configuration."
        }
        exit 0
    }

    $configureArgs = @(
        '-S', $projectRoot, '-B', $buildDir, '-G', 'NMake Makefiles JOM',
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=$jom",
        "-DCMAKE_BUILD_TYPE:STRING=$Configuration",
        "-DROBOT3D_QT_ROOT:PATH=$qtRoot",
        "-DROBOT3D_RUNTIME_DIR:PATH=$runDir"
    )
    if ($WithoutVTK)
    {
        $configureArgs += '-DROBOT3D_WITH_VTK=OFF'
    }
    else
    {
        if (-not $VtkDir)
        {
            $vtkMetadata = Join-Path $projectRoot "build\deps\vtk-sdk-$Configuration.json"
            if (Test-Path -LiteralPath $vtkMetadata)
            {
                $VtkDir = (Get-Content -LiteralPath $vtkMetadata -Raw -Encoding UTF8 | ConvertFrom-Json).vtkDir
            }
            else
            {
                $VtkDir = Join-Path $projectRoot "build\deps\vtk-9.2.6-v140-$Configuration\install\lib\cmake\vtk-9.2"
            }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $VtkDir 'vtk-config.cmake')))
        {
            throw "VTK SDK not found: $VtkDir. Run tools/build-vtk.ps1 -Configuration $Configuration first, or specify -VtkDir."
        }
        $configureArgs += @('-DROBOT3D_WITH_VTK=ON', "-DVTK_DIR:PATH=$VtkDir")
    }
    if ($WithoutOCCT)
    {
        $configureArgs += '-DROBOT3D_WITH_OCCT=OFF'
    }
    else
    {
        if (-not $OcctDir)
        {
            $occtMetadata = Join-Path $projectRoot "build\deps\occt-sdk-$Configuration.json"
            if (-not (Test-Path -LiteralPath $occtMetadata))
            {
                Invoke-Checked 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    (Join-Path $projectRoot 'tools\build-occt.ps1'), '-Configuration', $Configuration,
                    '-Jobs', "$Jobs", '-IfMissing')
            }
            if (Test-Path -LiteralPath $occtMetadata)
            {
                $OcctDir = (Get-Content -LiteralPath $occtMetadata -Raw -Encoding UTF8 | ConvertFrom-Json).occtDir
            }
        }
        if (-not $OcctDir)
        {
            throw 'OCCT SDK metadata missing. Run tools/build-occt.ps1 or specify -OcctDir.'
        }
        $configureArgs += @('-DROBOT3D_WITH_OCCT=ON', "-DOpenCASCADE_DIR:PATH=$OcctDir")
    }
    Write-Host "Configuring $Configuration / Qt $version / MSVC2015 x64 / VTK=$(-not $WithoutVTK) / OCCT=$(-not $WithoutOCCT)"
    Invoke-Checked $cmake $configureArgs
    if ($Action -eq 'Configure')
    {
        exit 0
    }

    $buildArgs = @('--build', $buildDir, '--parallel', "$Jobs")
    if ($Action -eq 'Rebuild')
    {
        $buildArgs += '--clean-first'
    }
    Invoke-Checked $cmake $buildArgs
    if (-not (Test-Path -LiteralPath $program -PathType Leaf))
    {
        throw "Build completed without the expected executable: $program"
    }
    Write-Host "Built: $program"
    Write-Host "Working directory: $runDir"
}
catch
{
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
