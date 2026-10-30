"""Fetch the pinned OCCT source tree; no application source is rewritten."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, sys
VERSION = '7.9.1'
TAG = 'V7_9_1'
COMMIT = '36c80dbe7b199348e2f0a2d6932a462a37fd0b90'
ORIGIN = 'https://github.com/Open-Cascade-SAS/OCCT.git'
def run(args):
    subprocess.check_call(args)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('root')
    args=parser.parse_args()
    root=pathlib.Path(args.root).resolve();root.mkdir(parents=True,exist_ok=True)
    source=root/('OCCT-src-'+VERSION)
    marker=source/'.robot3d-source-ready'
    if marker.is_file() and marker.read_text(encoding='utf-8').strip()==COMMIT and (source/'CMakeLists.txt').is_file():
        print('OCCT source already verified:',COMMIT,flush=True);return
    git=shutil.which('git')
    if not git:raise RuntimeError('Git is required to fetch the pinned sparse OCCT source tree')
    if not (source/'.git').is_dir():
        if source.exists() and any(source.iterdir()):raise RuntimeError('Source target is not an empty Git checkout: '+str(source))
        run([git,'-c','http.version=HTTP/1.1','-c','core.longpaths=true','clone','--filter=blob:none','--no-checkout','--depth','1','--branch',TAG,ORIGIN,str(source)])
    actual=subprocess.check_output([git,'-C',str(source),'rev-parse','--verify',TAG+'^{commit}']).decode('ascii').strip()
    if actual!=COMMIT:raise RuntimeError('Pinned OCCT tag/commit mismatch: '+actual)
    run([git,'-C',str(source),'config','core.sparseCheckout','true'])
    patterns=source/'.git'/'info'/'sparse-checkout';patterns.parent.mkdir(parents=True,exist_ok=True)
    patterns.write_text('/CMakeLists.txt\n/adm/\n/src/\n/LICENSE_LGPL_21.txt\n/OCCT_LGPL_EXCEPTION.txt\n/README.md\n',encoding='utf-8')
    run([git,'-C',str(source),'-c','http.version=HTTP/1.1','checkout','--detach',TAG])
    # Record revision and representative file hashes for repeatable dependency builds.
    hashes={}
    for rel in ['CMakeLists.txt','adm/MODULES','src/Standard/Standard_Version.hxx','LICENSE_LGPL_21.txt','OCCT_LGPL_EXCEPTION.txt']:
        p=source/rel
        if p.is_file():hashes[rel]=hashlib.sha256(p.read_bytes()).hexdigest()
    (root/'occt-source-lock.json').write_text(json.dumps({'version':VERSION,'tag':TAG,'commit':COMMIT,'origin':ORIGIN,'filesSha256':hashes},indent=2),encoding='utf-8')
    marker.write_text(COMMIT+'\n',encoding='utf-8')
    print('OCCT source verified:',source,COMMIT,flush=True)
if __name__=='__main__':main()
