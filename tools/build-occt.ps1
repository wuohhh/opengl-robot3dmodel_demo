[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateRange(1, 32)][int]$Jobs = 3,
    [string]$BuildRoot = '',
    [switch]$IfMissing
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = 'C:\Python\Python37\python.exe'
$cmake = 'C:\CMake\bin\cmake.exe'
$jom = 'C:\Qt\Qt5.9.9\Tools\QtCreator\bin\jom.exe'
$vcVars = 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat'
if (-not $BuildRoot) { $BuildRoot = Join-Path $projectRoot 'build\deps' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$source = Join-Path $projectRoot 'build\deps\OCCT-src-7.9.1'
$build = Join-Path $BuildRoot "occt-7.9.1-v140-$Configuration"
$install = Join-Path $build 'install'
$config = Join-Path $install 'lib\cmake\opencascade'
$record = Join-Path $projectRoot "build\deps\occt-sdk-$Configuration.json"
function Invoke-Checked {
    param([string]$Tool,[string[]]$ToolArguments)
    & $Tool @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed ($LASTEXITCODE)" }
}
$env:PYTHONIOENCODING = 'utf-8'
if ($IfMissing -and (Test-Path -LiteralPath (Join-Path $config 'OpenCASCADEConfig.cmake')) -and
    (Test-Path -LiteralPath (Join-Path $install 'bin\TKDESTEP.dll')) -and
    (Test-Path -LiteralPath (Join-Path $install 'lib\TKDEIGES.lib'))) {
    Write-Host "Reusing OCCT $Configuration SDK: $config"
    exit 0
}
Invoke-Checked $python @((Join-Path $PSScriptRoot 'fetch-occt.py'),(Join-Path $projectRoot 'build\deps'))
foreach ($required in @($cmake,$jom,$vcVars)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required tool missing: $required" }
}
$lines = & $env:ComSpec /d /s /c ('"' + $vcVars + '" amd64 >nul && set')
if ($LASTEXITCODE -ne 0) { throw 'VS2015 amd64 environment failed' }
foreach ($line in $lines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$env:VSLANG = '1033'
$compilerTemp = Join-Path $BuildRoot 'occt-compiler-temp'
New-Item -ItemType Directory -Path $compilerTemp -Force | Out-Null
$env:TEMP = $compilerTemp
$env:TMP = $compilerTemp
$argsForConfigure = @(
    '-S',$source,'-B',$build,'-G','NMake Makefiles JOM',
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=$jom",
    "-DCMAKE_BUILD_TYPE:STRING=$Configuration",
    "-DINSTALL_DIR:PATH=$install",'-DINSTALL_DIR_LAYOUT=Unix',
    '-DCMAKE_DEPENDS_USE_COMPILER=OFF',
    '-DCMAKE_TRY_COMPILE_PLATFORM_VARIABLES=CMAKE_DEPENDS_USE_COMPILER',
    '-DBUILD_CPP_STANDARD=C++11','-DBUILD_LIBRARY_TYPE=Shared',
    '-DBUILD_MODULE_FoundationClasses=OFF','-DBUILD_MODULE_ModelingData=OFF',
    '-DBUILD_MODULE_ModelingAlgorithms=OFF','-DBUILD_MODULE_Visualization=OFF',
    '-DBUILD_MODULE_ApplicationFramework=OFF','-DBUILD_MODULE_DataExchange=OFF',
    '-DBUILD_MODULE_DETools=OFF','-DBUILD_MODULE_Draw=OFF',
    '-DBUILD_ADDITIONAL_TOOLKITS:STRING=TKDESTEP;TKDEIGES;TKMesh;TKPrim',
    '-DBUILD_SAMPLES_QT=OFF','-DBUILD_SAMPLES_MFC=OFF','-DBUILD_Inspector=OFF',
    '-DUSE_TBB=OFF','-DUSE_FREETYPE=OFF','-DUSE_FREEIMAGE=OFF','-DUSE_OPENGL=OFF',
    '-DUSE_GLES2=OFF','-DUSE_VTK=OFF','-DUSE_TK=OFF','-DUSE_RAPIDJSON=OFF',
    '-DUSE_DRACO=OFF','-DUSE_OPENVR=OFF','-DUSE_FFMPEG=OFF',
    '-DBUILD_USE_PCH=OFF','-DBUILD_USE_VCPKG=OFF',
    '-DINSTALL_TEST_CASES=OFF','-DINSTALL_SAMPLES=OFF'
)
Invoke-Checked $cmake $argsForConfigure
Invoke-Checked $cmake @('--build',$build,'--parallel',"$Jobs")
Invoke-Checked $cmake @('--install',$build)
if (-not (Test-Path -LiteralPath (Join-Path $config 'OpenCASCADEConfig.cmake'))) {
    throw "OCCT package config not found: $config"
}
Invoke-Checked $python @('-c',
    "import json,sys; json.dump({'version':'7.9.1','sourceCommit':'36c80dbe7b199348e2f0a2d6932a462a37fd0b90','provenance':'local-source-build','configuration':sys.argv[3],'occtDir':sys.argv[2],'prefix':sys.argv[4]},open(sys.argv[1],'w',encoding='utf-8',newline=''),indent=2)",
    $record,$config,$Configuration,$install)
Write-Host "OCCT $Configuration SDK installed: $config"
