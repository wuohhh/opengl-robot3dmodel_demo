"""Stage a controlled, portable workpiece Release validation directory.

Only the PE dependency closure of the application/tests and required Qt plugins is
copied. The package is a local validation artifact, not a reviewed redistribution.
Existing output files are never overwritten or removed.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import runpy
import sys

QT_ROOT = pathlib.Path(r'C:\Qt\Qt5.9.9\5.9.9\msvc2015_64')
EXECUTABLES = ('robot3ddemo.exe', 'robot3d_workpiece_gui_tests.exe',
               'robot3d_workpiece_loader_tests.exe')
ALLOWED_OCCT_TOOLKITS = {
    'tkbo', 'tkbool', 'tkbrep', 'tkcaf', 'tkcdf', 'tkde', 'tkdeiges',
    'tkdestep', 'tkernel', 'tkg2d', 'tkg3d', 'tkgeomalgo', 'tkgeombase',
    'tkhlr', 'tklcaf', 'tkmath', 'tkmesh', 'tkprim', 'tkservice',
    'tkshhealing', 'tktopalgo', 'tkv3d', 'tkvcaf', 'tkxcaf', 'tkxsbase',
}


def within(path, parent):
    try:
        return os.path.normcase(os.path.commonpath([str(path), str(parent)])) == os.path.normcase(str(parent))
    except ValueError:
        return False


def index_files(folder, extension):
    if not folder.is_dir():
        raise RuntimeError('Required directory is missing: ' + str(folder))
    result = {}
    for name in os.listdir(str(folder)):
        path = folder / name
        if path.is_file() and name.lower().endswith(extension):
            key = name.lower()
            if key in result:
                raise RuntimeError('Case-insensitive filename collision: ' + str(path))
            result[key] = path
    return result


def read_cache(path):
    if not path.is_file():
        raise RuntimeError('CMake cache is missing: ' + str(path))
    values = {}
    with open(str(path), 'r', encoding='utf-8-sig', newline='') as stream:
        for line in stream:
            if line.startswith(('#', '//')) or '=' not in line or ':' not in line.split('=', 1)[0]:
                continue
            key, value = line.rstrip('\r\n').split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def sha256_file(path):
    digest = hashlib.sha256()
    with open(str(path), 'rb') as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def add_tree(plan, source, destination, category):
    if not source.is_dir():
        raise RuntimeError('Required deployed data directory is missing: ' + str(source))
    count = 0
    for folder, directories, files in os.walk(str(source), followlinks=False):
        folder_path = pathlib.Path(folder)
        for name in directories:
            if (folder_path / name).is_symlink():
                raise RuntimeError('Symlink data directory is not staged: ' + str(folder_path / name))
        for name in files:
            path = folder_path / name
            if path.is_symlink():
                raise RuntimeError('Symlink data file is not staged: ' + str(path))
            relative = path.relative_to(source)
            plan_file(plan, path, destination / relative, category)
            count += 1
    if not count:
        raise RuntimeError('Required deployed data directory is empty: ' + str(source))


def plan_file(plan, source, destination, category):
    key = destination.as_posix().lower()
    if key in plan:
        previous = plan[key]
        if previous['source'] != source and sha256_file(previous['source']) != sha256_file(source):
            raise RuntimeError('Conflicting staged file: ' + destination.as_posix())
        return
    if not source.is_file():
        raise RuntimeError('Required source file is missing: ' + str(source))
    plan[key] = {'source': source, 'relative': destination, 'category': category}


def check_toolkit(name):
    stem = pathlib.Path(name).stem.lower()
    if stem.startswith('tk') and stem not in ALLOWED_OCCT_TOOLKITS:
        raise RuntimeError('Unexpected OCCT toolkit in runtime closure: ' + name +
                           '; DRAW, Viewer/OpenGL, inspector and unused toolkits must not be staged')


def runtime_plan(build_bin, imports):
    available = index_files(build_bin, '.dll')
    qt_bin = QT_ROOT / 'bin'
    qt_dlls = index_files(qt_bin, '.dll')
    system = pathlib.Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32'
    system_dlls = index_files(system, '.dll')
    plan = {}
    pending = []
    for name in EXECUTABLES:
        path = build_bin / name
        plan_file(plan, path, pathlib.Path('bin') / name, 'application-or-test-executable')
        pending.append(path)

    plugin_root = QT_ROOT / 'plugins'
    windows = plugin_root / 'platforms' / 'qwindows.dll'
    plan_file(plan, windows, pathlib.Path('plugins/platforms/qwindows.dll'), 'qt-platform-plugin')
    pending.append(windows)
    images = index_files(plugin_root / 'imageformats', '.dll')
    for key, path in sorted(images.items()):
        # Debug plugins have a sibling with the same basename minus the trailing d.
        if path.stem.lower().endswith('d') and (path.stem[:-1] + '.dll').lower() in images:
            continue
        plan_file(plan, path, pathlib.Path('plugins/imageformats') / path.name, 'qt-image-plugin')
        pending.append(path)
    # Qt may load these dynamically, so they cannot be found only from executable imports.
    for name in ('libEGL.dll', 'libGLESv2.dll', 'd3dcompiler_47.dll'):
        path = qt_dlls.get(name.lower())
        if path is not None:
            plan_file(plan, path, pathlib.Path('bin') / path.name, 'qt-optional-angle-runtime')
            pending.append(path)

    visited = set()
    system_dependencies = {}
    while pending:
        path = pending.pop(0)
        key = os.path.normcase(str(path.resolve()))
        if key in visited:
            continue
        visited.add(key)
        check_toolkit(path.name)
        dependencies = imports(str(path))
        if path.suffix.lower() in ('.dll', '.exe'):
            with open(str(path), 'rb') as stream:
                if stream.read(2) != b'MZ':
                    raise RuntimeError('Runtime file is not a readable PE image: ' + str(path))
        for name in dependencies:
            low = name.lower()
            check_toolkit(name)
            if low.startswith(('api-ms-', 'ext-ms-')):
                system_dependencies[low] = 'Windows API set'
                continue
            dependency = available.get(low) or qt_dlls.get(low)
            if dependency is None:
                dependency = system_dlls.get(low)
                if dependency is not None:
                    system_dependencies[low] = str(dependency)
                    continue
                raise RuntimeError(str(path) + ' requires missing runtime DLL: ' + name)
            plan_file(plan, dependency, pathlib.Path('bin') / dependency.name, 'runtime-dll')
            pending.append(dependency)
    return plan, system_dependencies


def copy_verified(source, destination):
    if destination.exists():
        raise RuntimeError('Refusing to overwrite staged file: ' + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    with open(str(source), 'rb') as src, open(str(destination), 'xb') as dst:
        while True:
            chunk = src.read(1024 * 1024)
            if not chunk:
                break
            dst.write(chunk)
            digest.update(chunk)
    result = digest.hexdigest()
    if sha256_file(destination) != result:
        raise RuntimeError('Staged file read-back mismatch: ' + str(destination))
    return result


def write_generated(path, text):
    normalized = re.sub(r'\r+\n|\r', '\n', text).replace('\n', '\r\n')
    encoding = 'utf-8-sig' if path.suffix.lower() == '.ps1' else 'utf-8'
    data = normalized.encode(encoding)
    if path.exists():
        raise RuntimeError('Refusing to overwrite generated file: ' + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(str(path), 'x', encoding=encoding, newline='') as stream:
        stream.write(normalized)
    if path.read_bytes() != data:
        raise RuntimeError('Generated file read-back mismatch: ' + str(path))
    return hashlib.sha256(data).hexdigest()


def launcher():
    return r'''[CmdletBinding()]
param(
    [ValidateSet('vtk', 'gl')][string]$Renderer = 'vtk',
    [ValidateSet('App', 'GuiTests', 'LoaderTests')][string]$Mode = 'App',
    [switch]$Lifecycle
)
$ErrorActionPreference = 'Stop'
$packageRoot = $PSScriptRoot
$manifestPath = Join-Path $packageRoot 'manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw '部署清单缺失：manifest.json。请重新部署完整验证包。'
}
try {
    $packageManifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
catch {
    throw '无法读取部署清单 manifest.json。请重新部署完整验证包。'
}
if (-not $packageManifest.files) {
    throw '部署清单没有文件记录。请重新部署完整验证包。'
}
$packagePrefix = [IO.Path]::GetFullPath($packageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
foreach ($entry in $packageManifest.files) {
    $relative = [string]$entry.path
    if ([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative)) {
        throw '部署清单包含无效的相对路径。请重新部署完整验证包。'
    }
    $required = [IO.Path]::GetFullPath((Join-Path $packageRoot $relative))
    if (-not $required.StartsWith($packagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw ('部署清单路径越界：' + $relative + '。请重新部署完整验证包。')
    }
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw ('部署文件缺失：' + $relative + '。请重新部署完整验证包。')
    }
}
$packageBin = Join-Path $packageRoot 'bin'
$env:PATH = (Join-Path $env:SystemRoot 'System32') + ';' + $packageBin
$env:QT_PLUGIN_PATH = Join-Path $packageRoot 'plugins'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $packageRoot 'plugins\platforms'
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_DISABLE_SHADER_DISK_CACHE = '1'
$env:QT_OPENGL = 'desktop'
$env:DEMO_RENDERER = if ($Renderer -eq 'gl') { 'opengl' } else { 'vtk' }
$occtResources = Join-Path $packageBin 'occt-resources'
$resourceVariables = @{
    CSF_XSMessage = 'XSMessage'
    CSF_SHMessage = 'SHMessage'
    CSF_XCAFDefaults = 'StdResource'
    CSF_StandardDefaults = 'StdResource'
    CSF_StandardLiteDefaults = 'StdResource'
    CSF_PluginDefaults = 'StdResource'
    CSF_IGESDefaults = 'XSTEPResource'
    CSF_STEPDefaults = 'XSTEPResource'
    CSF_XmlOcafResource = 'XmlOcafResource'
}
foreach ($name in $resourceVariables.Keys) {
    $value = Join-Path $occtResources $resourceVariables[$name]
    if (Test-Path -LiteralPath $value -PathType Container) {
        [Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }
}
$exitCode = 1
Push-Location -LiteralPath $packageRoot
try {
    if ($Mode -eq 'GuiTests') {
        $testArgs = @($packageRoot, (Join-Path $packageRoot 'validation\gui'))
        if ($Lifecycle) { $testArgs += '--lifecycle' }
        & (Join-Path $packageBin 'robot3d_workpiece_gui_tests.exe') @testArgs
    }
    elseif ($Mode -eq 'LoaderTests') {
        & (Join-Path $packageBin 'robot3d_workpiece_loader_tests.exe') (Join-Path $packageRoot 'validation\loader')
    }
    else {
        & (Join-Path $packageBin 'robot3ddemo.exe')
    }
    $exitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $exitCode
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_dir', help='Configured and built Release directory')
    parser.add_argument('output_dir', help='Empty output directory under this workspace build/')
    args = parser.parse_args()
    workspace = pathlib.Path(__file__).resolve().parent.parent
    workspace_build = (workspace / 'build').resolve()
    build = pathlib.Path(args.build_dir).resolve()
    output = pathlib.Path(args.output_dir).resolve()
    if not within(output, workspace_build) or output == workspace_build:
        raise RuntimeError('Output must be a dedicated subdirectory of workspace build/: ' + str(output))
    if within(output, build) or within(build, output):
        raise RuntimeError('Output and build directory must not overlap or be ancestors: ' + str(output))
    if output.exists():
        if not output.is_dir():
            raise RuntimeError('Output already exists and is not a directory: ' + str(output))
        for folder, directories, files in os.walk(str(output), followlinks=False):
            if files or any((pathlib.Path(folder) / name).is_symlink() for name in directories):
                raise RuntimeError('Output contains existing files or links; choose a new empty directory: ' + str(output))
    cache = read_cache(build / 'CMakeCache.txt')
    if cache.get('CMAKE_BUILD_TYPE', '').lower() != 'release':
        raise RuntimeError('This validation packager requires a Release build and Release Qt plugins')
    if cache.get('ROBOT3D_WITH_OCCT', '').upper() not in ('ON', 'TRUE', '1'):
        raise RuntimeError('A full workpiece validation package requires OCCT enabled')
    build_bin = build / 'bin'
    imports = runpy.run_path(str(workspace / 'tools' / 'fetch-occt-sdk-runtime.py'))['imports']
    plan, system_dependencies = runtime_plan(build_bin, imports)
    runtime_value = cache.get('ROBOT3D_RUNTIME_DIR')
    if not runtime_value:
        raise RuntimeError('ROBOT3D_RUNTIME_DIR is missing from the CMake cache')
    runtime = pathlib.Path(runtime_value)
    if not runtime.is_absolute():
        runtime = (build / runtime).resolve()
    add_tree(plan, runtime / 'robotType', pathlib.Path('robotType'), 'application-model-data')
    add_tree(plan, build_bin / 'licenses', pathlib.Path('bin/licenses'), 'license-material')
    add_tree(plan, build_bin / 'occt-resources', pathlib.Path('bin/occt-resources'), 'occt-runtime-resource')
    # UI icons and shaders are compiled into demo.qrc, not loose runtime files.
    for candidate in (runtime / 'resources', runtime / 'resource'):
        if candidate.is_dir():
            add_tree(plan, candidate, pathlib.Path(candidate.name), 'additional-runtime-data')

    # Validate the entire plan before creating output; never delete or clean outputs.
    for item in plan.values():
        target = (output / item['relative']).resolve()
        if not within(target, output):
            raise RuntimeError('Invalid destination outside output directory: ' + str(target))
        if target.exists():
            raise RuntimeError('Output file already exists: ' + str(target))
    output.mkdir(parents=True, exist_ok=True)
    manifest_files = []
    for key in sorted(plan):
        item = plan[key]
        target = output / item['relative']
        digest = copy_verified(item['source'], target)
        manifest_files.append({
            'path': item['relative'].as_posix(),
            'sha256': digest,
            'size': target.stat().st_size,
            'source': str(item['source']),
            'category': item['category'],
        })
    generated = [
        (pathlib.Path('bin/qt.conf'), '[Paths]\nPrefix=..\nPlugins=plugins\n'),
        (pathlib.Path('launch-workpiece.ps1'), launcher()),
        (pathlib.Path('VALIDATION-README.txt'),
         'Controlled local workpiece Release validation artifact.\n'
         'Launch with powershell -NoProfile -ExecutionPolicy Bypass -File launch-workpiece.ps1\n'
         'Optional: -Renderer gl / -Mode GuiTests / -Mode LoaderTests / -Lifecycle\n'
         'PATH is limited to System32 and package bin; system DLLs are host prerequisites.\n'
         'This artifact has not been approved as a commercial redistribution or license review.\n'
         'Source models/resources are read through the authorized Python process. Copied test data\n'
         'may be plaintext; this package does not certify or restore enterprise encryption status.\n'),
    ]
    for relative, text in generated:
        target = output / relative
        digest = write_generated(target, text)
        manifest_files.append({
            'path': relative.as_posix(), 'sha256': digest,
            'size': target.stat().st_size,
            'source': 'generated by tools/stage-workpiece-release.py',
            'category': 'generated-validation-file',
        })
    manifest = {
        'format': 'robot3d-workpiece-validation-manifest-1',
        'purpose': 'controlled-local-validation-only',
        'configuration': 'Release',
        'inputBuildDirectory': str(build),
        'qtRoot': str(QT_ROOT),
        'sourceReadChannel': r'C:\Python\Python37\python.exe (enterprise authorized process)',
        'encryptionNotice': 'Controlled test copies may be plaintext; no encryption or redistribution approval is implied.',
        'systemDependenciesNotCopied': dict(sorted(system_dependencies.items())),
        'files': sorted(manifest_files, key=lambda item: item['path'].lower()),
        'manifestSelfHash': 'Not included; a manifest cannot contain its own final SHA256.',
    }
    write_generated(output / 'manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print('Staged validation directory: ' + str(output))
    print('Verified files: ' + str(len(manifest_files)) + '; host system dependencies: ' + str(len(system_dependencies)))
    print('Controlled test copies may be plaintext. This is not a commercially reviewed release package.')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('Workpiece staging failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
