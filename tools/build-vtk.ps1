[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [ValidateRange(1,64)][int]$Jobs=[Math]::Min(4,[Environment]::ProcessorCount),
    [string]$BuildRoot=''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$python='C:\Python\Python37\python.exe'
$qtRoot='C:\Qt\Qt5.9.9\5.9.9\msvc2015_64'
$vcVars='C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat'
$cmake='C:\CMake\bin\cmake.exe'
$jom='C:\Qt\Qt5.9.9\Tools\QtCreator\bin\jom.exe'
$source=Join-Path $projectRoot 'build\deps\VTK-9.2.6'
$localRoot=Join-Path $projectRoot "build\deps\vtk-build-root.json"
if(-not $BuildRoot -and (Test-Path -LiteralPath $localRoot)) { $BuildRoot=(Get-Content -LiteralPath $localRoot -Raw -Encoding UTF8 | ConvertFrom-Json).buildRoot }
if(-not $BuildRoot) { $BuildRoot=Join-Path $projectRoot "build\deps" }
$BuildRoot=[IO.Path]::GetFullPath($BuildRoot)
$build=Join-Path $BuildRoot "vtk-9.2.6-v140-$Configuration"
$install=Join-Path $build 'install'
function Invoke-Checked {
    param([string]$Tool,[string[]]$ToolArguments)
    & $Tool @ToolArguments
    if($LASTEXITCODE -ne 0) { throw "$Tool failed ($LASTEXITCODE)" }
}
$env:PYTHONIOENCODING='utf-8'
Invoke-Checked $python @((Join-Path $PSScriptRoot 'fetch-vtk.py'),(Join-Path $projectRoot 'build\deps'))
Invoke-Checked $python @((Join-Path $PSScriptRoot 'patch-vtk-v140.py'),$source)
$lines=& $env:ComSpec /d /s /c ('"'+$vcVars+'" amd64 >nul && set')
if($LASTEXITCODE -ne 0) { throw 'VS2015 amd64 environment failed' }
foreach($line in $lines) {
    if($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$env:PATH=(Join-Path $qtRoot 'bin')+';'+$env:PATH
$env:VSLANG='1033'
$compilerTemp=Join-Path $BuildRoot 'compiler-temp'
New-Item -ItemType Directory -Path $compilerTemp -Force | Out-Null
$env:TEMP=$compilerTemp
$env:TMP=$compilerTemp
$vtkConfigureArgs=@(
    '-S',$source,'-B',$build,'-G','NMake Makefiles JOM',
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=$jom","-DCMAKE_BUILD_TYPE:STRING=$Configuration",
    "-DCMAKE_INSTALL_PREFIX:PATH=$install","-DCMAKE_PREFIX_PATH:PATH=$qtRoot",
    '-DCMAKE_DEPENDS_USE_COMPILER=OFF',
    '-DCMAKE_TRY_COMPILE_PLATFORM_VARIABLES=CMAKE_DEPENDS_USE_COMPILER',
    '-DBUILD_SHARED_LIBS=ON','-DVTK_BUILD_TESTING=OFF','-DVTK_BUILD_EXAMPLES=OFF',
    '-DVTK_BUILD_ALL_MODULES=OFF','-DVTK_ENABLE_WRAPPING=OFF','-DVTK_WRAP_PYTHON=OFF','-DVTK_WRAP_JAVA=OFF',
    '-DVTK_GROUP_ENABLE_StandAlone=DONT_WANT','-DVTK_GROUP_ENABLE_Rendering=DONT_WANT',
    '-DVTK_GROUP_ENABLE_Qt=DONT_WANT','-DVTK_QT_VERSION=5',
    '-DVTK_MODULE_ENABLE_VTK_GUISupportQt=YES',
    '-DVTK_MODULE_ENABLE_VTK_RenderingOpenGL2=YES',
    '-DVTK_MODULE_ENABLE_VTK_InteractionStyle=YES',
    '-DVTK_MODULE_ENABLE_VTK_FiltersSources=YES',
    '-DVTK_MODULE_ENABLE_VTK_FiltersCore=YES',
    '-DVTK_SMP_IMPLEMENTATION_TYPE=Sequential'
)
Invoke-Checked $cmake $vtkConfigureArgs
Invoke-Checked $cmake @('--build',$build,'--parallel',"$Jobs")
Invoke-Checked $cmake @('--install',$build)
Write-Host "VTK SDK installed: $install"
$sdkMetadata=Join-Path $projectRoot "build\deps\vtk-sdk-$Configuration.json"
$vtkConfig=Join-Path $install "lib\cmake\vtk-9.2"
Invoke-Checked $python @('-c',"import json,sys; json.dump({'vtkDir':sys.argv[2]},open(sys.argv[1],'w',encoding='utf-8',newline=''),ensure_ascii=False)",$sdkMetadata,$vtkConfig)
Invoke-Checked $python @('-c',"import json,sys; json.dump({'buildRoot':sys.argv[2]},open(sys.argv[1],'w',encoding='utf-8',newline=''),ensure_ascii=False)",$localRoot,$BuildRoot)
