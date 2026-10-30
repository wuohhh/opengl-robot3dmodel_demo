"""Fetch and stage Microsoft-signed Debug CRT for local OCCT development tests.

No installer runs and no System32/global file is modified. These nonredistributable
Debug binaries must never enter Release or customer distribution packages.
"""
import argparse
import base64
import hashlib
import json
import os
import pathlib
import runpy
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

PAYLOAD_URL = ('https://download.visualstudio.microsoft.com/download/pr/'
               '45d3b8dd-bced-4b37-9974-142f748d710c/'
               '4aaf54db0bfc9435f7c3660e1a00237a4b556042bfeea64bde44c2e0194e6ee5/'
               'Microsoft.VC.14.44.17.14.CRT.Redist.X64.base.vsix')
PAYLOAD_SHA256 = '4aaf54db0bfc9435f7c3660e1a00237a4b556042bfeea64bde44c2e0194e6ee5'
CATALOG_DECLARED_PAYLOAD_SIZE = 3236248
CATALOG_URL = ('https://download.visualstudio.microsoft.com/download/pr/'
               '6bd63d7f-1818-4e03-a552-9feb11496c19/'
               '4cc372d79f02e331c999a926b833a49eb74b30357d30f41ba9cffa1dc0369379/'
               'VisualStudio.vsman')
CATALOG_EXPECTED_SHA256 = '4cc372d79f02e331c999a926b833a49eb74b30357d30f41ba9cffa1dc0369379'
CATALOG_OBSERVED_SHA256 = '963760b01f621ed4fa40d1cf97aeb042f7bc6e04e3176c13c38abb19d265f789'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def within(path, parent):
    try:
        return os.path.normcase(os.path.commonpath([str(path), str(parent)])) == os.path.normcase(str(parent))
    except ValueError:
        return False


def write_verified(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(str(path), 'wb') as stream:
        stream.write(data)
    if path.read_bytes() != data:
        raise RuntimeError('Read-back verification failed: ' + str(path))


def fetch_payload(path):
    if path.is_file():
        data = path.read_bytes()
        if sha256(data) == PAYLOAD_SHA256:
            return data
        raise RuntimeError('Existing pinned payload failed SHA256; not overwritten: ' + str(path))
    last = None
    for attempt in range(4):
        try:
            request = urllib.request.Request(PAYLOAD_URL, headers={
                'User-Agent': 'robot3d-development-debug-crt',
                'Accept-Encoding': 'identity',
            })
            with urllib.request.urlopen(request, timeout=35) as response:
                if urllib.parse.urlparse(response.url).hostname != 'download.visualstudio.microsoft.com':
                    raise RuntimeError('Payload redirected outside the official Microsoft download host')
                data = response.read(16 * 1024 * 1024)
            if sha256(data) != PAYLOAD_SHA256:
                raise RuntimeError('Microsoft payload SHA256 mismatch')
            write_verified(path, data)
            return data
        except Exception as error:
            last = error
            print('Microsoft payload retry', attempt + 1, type(error).__name__, flush=True)
    raise last


def microsoft_signatures(paths):
    literals = ','.join("'" + str(path).replace("'", "''") + "'" for path in paths)
    script = (r"$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';"
              r"$env:PSModulePath=Join-Path $PSHOME 'Modules';"
              r"Import-Module Microsoft.PowerShell.Security;"
              r"[Console]::OutputEncoding=[Text.Encoding]::UTF8;"
              r"$items=@(" + literals + r"); $items | ForEach-Object {"
              r"$s=Get-AuthenticodeSignature -LiteralPath $_;"
              r"$v=(Get-Item -LiteralPath $_).VersionInfo.FileVersion;"
              r"[PSCustomObject]@{Path=$_;Status=$s.Status.ToString();"
              r"Subject=$s.SignerCertificate.Subject;"
              r"Thumbprint=$s.SignerCertificate.Thumbprint;"
              r"TimestampSubject=$s.TimeStamperCertificate.Subject;FileVersion=$v}"
              r"} | ConvertTo-Json -Depth 4")
    command = base64.b64encode(script.encode('utf-16le')).decode('ascii')
    raw = subprocess.check_output(['powershell.exe', '-NoProfile', '-NonInteractive',
                                   '-EncodedCommand', command])
    records = json.loads(raw.decode('utf-8-sig'))
    if isinstance(records, dict):
        records = [records]
    by_name = {}
    for record in records:
        if record.get('Status') != 'Valid' or 'O=Microsoft Corporation' not in (record.get('Subject') or ''):
            raise RuntimeError('Debug DLL lacks valid Microsoft Authenticode signature: ' + record['Path'])
        by_name[pathlib.Path(record['Path']).name.lower()] = record
    return by_name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sdk', help='Controlled official OCCT SDK root under workspace build/deps')
    args = parser.parse_args()
    workspace = pathlib.Path(__file__).resolve().parent.parent
    deps = (workspace / 'build' / 'deps').resolve()
    sdk = pathlib.Path(args.sdk).resolve()
    if not within(sdk, deps):
        raise RuntimeError('Debug CRT is restricted to this workspace build/deps SDK')
    bind = sdk / 'win64' / 'vc14' / 'bind'
    if not bind.is_dir():
        raise RuntimeError('OCCT Debug DLL directory is missing: ' + str(bind))
    payload = deps / pathlib.Path(urllib.parse.urlparse(PAYLOAD_URL).path).name
    data = fetch_payload(payload)
    extract = deps / 'msvc-debug-crt-14.44.35112'
    available, members = {}, {}
    with zipfile.ZipFile(str(payload)) as archive:
        for entry in archive.infolist():
            if '/debug_nonredist/x64/Microsoft.VC143.DebugCRT/' not in entry.filename or not entry.filename.lower().endswith('.dll'):
                continue
            name = pathlib.PurePosixPath(entry.filename).name
            path = extract / name
            contents = archive.read(entry)
            write_verified(path, contents)
            available[name.lower()] = path
            members[name.lower()] = entry.filename
    if 'vcruntime140_1d.dll' not in available:
        raise RuntimeError('Pinned Microsoft payload does not contain the required x64 Debug CRT')
    # Validate signatures before parsing or staging any downloaded DLL.
    signatures = microsoft_signatures(list(available.values()))
    imports = runpy.run_path(str(workspace / 'tools' / 'fetch-occt-sdk-runtime.py'))['imports']
    needed = set()
    for name in os.listdir(str(bind)):
        if name.lower().startswith('tk') and name.lower().endswith('.dll'):
            needed.update(dep.lower() for dep in imports(str(bind / name)) if dep.lower() in available)
    pending = list(needed)
    while pending:
        name = pending.pop(0)
        for dependency in imports(str(available[name])):
            low = dependency.lower()
            if low in available and low not in needed:
                needed.add(low)
                pending.append(low)
    if 'vcruntime140_1d.dll' not in needed:
        raise RuntimeError('Required OCCT toolkit dependency was not found; refusing unrelated CRT staging')
    records = []
    for name in sorted(needed):
        source = available[name]
        destination = bind / source.name
        contents = source.read_bytes()
        write_verified(destination, contents)
        record = dict(signatures[name])
        record.update({
            'name': source.name, 'sha256': sha256(contents),
            'size': len(contents), 'sourceMember': members[name],
            'destination': str(destination),
        })
        records.append(record)
    provenance = {
        'purpose': 'local-development-tests-only',
        'redistributable': False,
        'globalInstallationPerformed': False,
        'payloadUrl': PAYLOAD_URL,
        'payloadSha256': sha256(data),
        'payloadHashMatched': True,
        'catalogDeclaredPayloadSize': CATALOG_DECLARED_PAYLOAD_SIZE,
        'actualPayloadSize': len(data),
        'payloadSizeMatched': len(data) == CATALOG_DECLARED_PAYLOAD_SIZE,
        'discoveryCatalog': {
            'url': CATALOG_URL,
            'channelExpectedSha256': CATALOG_EXPECTED_SHA256,
            'observedSha256': CATALOG_OBSERVED_SHA256,
            'channelHashMatched': False,
            'note': 'Official HTTPS discovery only. Catalog pin did not match; payload SHA and Microsoft DLL signatures were independently required.',
        },
        'files': records,
        'documentation': 'https://learn.microsoft.com/en-us/cpp/windows/preparing-a-test-machine-to-run-a-debug-executable?view=msvc-170',
    }
    encoded = (json.dumps(provenance, ensure_ascii=False, indent=2) + '\n').replace('\n', '\r\n').encode('utf-8')
    write_verified(sdk / 'debug-crt-provenance.json', encoded)
    print('Microsoft-signed Debug CRT staged for development only:', ', '.join(sorted(needed)))
    print('No installer was run; no System32 files were modified.')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('Debug CRT staging failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
