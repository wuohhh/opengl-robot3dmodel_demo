"""Fetch pinned VTK source into the ignored build dependency directory."""
import concurrent.futures
import hashlib
import os
import sys
import tarfile
import urllib.request

URL = 'https://www.vtk.org/files/release/9.2/VTK-9.2.6.tar.gz'
TOTAL = 53810904
CHUNK = 4 * 1024 * 1024
# The archive digest is verified before unpacking (filled from the official archive).
EXPECTED_SHA256 = '06fc8d49c4e56f498c40fcb38a563ed8d4ec31358d0101e8988f0bb4d539dd12'
root = os.path.abspath(sys.argv[1])
os.makedirs(root, exist_ok=True)
archive = os.path.join(root, 'VTK-official-9.2.6.tar.gz')
source = os.path.join(root, 'VTK-9.2.6')
marker = os.path.join(source, '.robot3d-source-ready')


def fetch(index):
    start = index * CHUNK
    end = min(TOTAL, start + CHUNK) - 1
    path = os.path.join(root, 'vtk-9.2.6.part%03d' % index)
    if os.path.isfile(path) and os.path.getsize(path) == end - start + 1:
        return path
    for attempt in range(4):
        try:
            request = urllib.request.Request(URL, headers={'Range': 'bytes=%d-%d' % (start, end)})
            with urllib.request.urlopen(request, timeout=60) as response:
                if response.status != 206:
                    raise RuntimeError('Server did not return the requested archive range')
                with open(path, 'wb') as output:
                    while True:
                        data = response.read(256 * 1024)
                        if not data:
                            break
                        output.write(data)
            if os.path.getsize(path) != end - start + 1:
                raise RuntimeError('Incomplete archive chunk')
            print('VTK source part %d ready' % index, flush=True)
            return path
        except Exception:
            if attempt == 3:
                raise


if not os.path.isfile(archive):
    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
        parts = list(pool.map(fetch, range((TOTAL + CHUNK - 1) // CHUNK)))
    temporary = archive + '.download'
    with open(temporary, 'wb') as output:
        for path in parts:
            with open(path, 'rb') as part:
                output.write(part.read())
    os.replace(temporary, archive)
digest = hashlib.sha256(open(archive, 'rb').read()).hexdigest()
if os.path.getsize(archive) != TOTAL or (EXPECTED_SHA256 and digest != EXPECTED_SHA256):
    raise RuntimeError('VTK archive size/checksum mismatch: ' + digest)
if os.path.isfile(marker) and open(marker, 'r').read().strip() == digest:
    print('VTK source already verified:', digest)
    sys.exit(0)
with tarfile.open(archive, 'r:gz') as package:
    for member in package:
        destination = os.path.abspath(os.path.join(root, member.name))
        if os.path.commonpath([root, destination]) != root:
            raise RuntimeError('Archive path escapes dependency directory')
        if member.issym() or member.islnk():
            continue
        if any(p in member.name.split('/') for p in ('Testing', 'Examples', 'Documentation', '.gitlab')):
            continue
        package.extract(member, root)
with open(marker, 'w', encoding='utf-8', newline='') as output:
    output.write(digest + '\n')
print('VTK source verified and extracted:', digest, flush=True)
