"""Fetch pinned official OCCT Windows SDK members using HTTP ranges and ZIP CRC checks.
Archives stay on official GitHub release storage; no third-party mirrors are used.
"""
import argparse, hashlib, io, json, os, struct, sys, time, urllib.request, zipfile, zlib

ASSETS = {'thirdparty': (257073494,193789890), 'debug': (257073722,386389286)}
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs): return None

def request(url, headers=None, redirect=True):
    last=None
    for attempt in range(4):
        try:
            req=urllib.request.Request(url,headers=dict({'User-Agent':'robot3d-occt-sdk'},**(headers or {})))
            return (urllib.request.urlopen(req,timeout=25) if redirect else
                    urllib.request.build_opener(NoRedirect).open(req,timeout=25))
        except urllib.error.HTTPError as e:
            if not redirect and e.code in (301,302,303,307,308): return e
            last=e
        except Exception as e: last=e
        print('retry',attempt+1,type(last).__name__,flush=True)
    raise last

class RemoteZip(io.RawIOBase):
    def __init__(self,asset,size):
        self.size=size;self.pos=0;self.asset=asset
        self.refresh_url()
    def refresh_url(self):
        asset=self.asset
        u='https://api.github.com/repos/Open-Cascade-SAS/OCCT/releases/assets/%d?download=1&nonce=%s'%(asset,time.time())
        with request(u,{'Accept':'application/octet-stream','Range':'bytes=0-0'},False) as r:
            self.url=r.headers.get('Location')
            if not self.url: raise RuntimeError('Official asset redirect missing: '+str(r.status))
        print('Official asset',asset,'redirect received',flush=True)
    def seek(self,n,w=0):
        self.pos=n if w==0 else self.pos+n if w==1 else self.size+n
        return self.pos
    def tell(self): return self.pos
    def read(self,n=-1):
        n=self.size-self.pos if n<0 else min(n,self.size-self.pos)
        if n<=0:return b''
        cache=os.path.join(os.path.dirname(__file__),'..','build','deps','occt-http-cache');os.makedirs(cache,exist_ok=True)
        def block(start,count):
            key=os.path.join(cache,'%s-%d-%d.bin'%(self.asset,start,count))
            b=open(key,'rb').read() if os.path.isfile(key) else b''
            if len(b)==count:return b
            last=None
            for attempt in range(5):
                try:
                    with request(self.url,{'Range':'bytes=%d-%d'%(start,start+count-1)}) as r:
                        if r.status!=206:raise RuntimeError('Server does not support byte ranges')
                        b=r.read(count+1)
                    if len(b)!=count:raise RuntimeError('Incomplete HTTP range')
                    open(key,'wb').write(b);return b
                except Exception as e:
                    last=e;print('range retry',start,count,attempt+1,type(e).__name__,flush=True)
                    if isinstance(e,urllib.error.HTTPError) and e.code in (401,403,404):self.refresh_url()
            raise last
        from concurrent.futures import ThreadPoolExecutor
        jobs=[(self.pos+i,min(262144,n-i)) for i in range(0,n,262144)]
        with ThreadPoolExecutor(max_workers=4) as pool:
            futures=[pool.submit(block,*job) for job in jobs]
            data=b''.join(f.result() for f in futures)
        self.pos+=len(data);return data

def imports(path):
    b=open(path,'rb').read()
    if b[:2]!=b'MZ': return []
    pe=struct.unpack_from('<I',b,60)[0]
    count=struct.unpack_from('<H',b,pe+6)[0]; opt=pe+24
    magic=struct.unpack_from('<H',b,opt)[0]; dd=opt+(112 if magic==0x20b else 96)
    rva=struct.unpack_from('<I',b,dd+8)[0]
    sections=opt+struct.unpack_from('<H',b,pe+20)[0]
    def offset(r):
        for i in range(count):
            virtualsize,va,rawsize,ptr=struct.unpack_from('<IIII',b,sections+40*i+8)
            if va<=r<va+max(virtualsize,rawsize):return ptr+r-va
        raise ValueError('Invalid PE RVA')
    if not rva:return []
    p=offset(rva); names=[]
    while any(b[p:p+20]):
        nr=struct.unpack_from('<I',b,p+12)[0];o=offset(nr)
        names.append(b[o:b.index(b'\0',o)].decode('ascii'));p+=20
    return names

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('sdk')
    ap.add_argument('--list',choices=ASSETS);ap.add_argument('--configuration',choices=['Release','Debug'],default='Release')
    args=ap.parse_args();sdk=os.path.abspath(args.sdk)
    kind=args.list or 'thirdparty'; asset,size=ASSETS[kind];remote=RemoteZip(asset,size)
    z=zipfile.ZipFile(remote)
    if args.list:
        for e in z.infolist():
            if e.filename.lower().endswith(('.dll','.lib')) or any(x in e.filename.lower() for x in ['license','copying']):
                print(e.filename,e.file_size)
        return
    bindir=os.path.join(sdk,'win64','vc14','bind' if args.configuration=='Debug' else 'bin')
    all_local={x.lower():os.path.join(bindir,x) for x in os.listdir(bindir) if x.lower().endswith('.dll')}
    entries={}
    for e in z.infolist():
        if e.filename.lower().endswith('.dll'):
            entries.setdefault(os.path.basename(e.filename).lower(),[]).append(e)
    manifest=[];queue=['TKDESTEP.dll','TKDEIGES.dll','TKMesh.dll','TKXCAF.dll','TKPrim.dll'];done=set()
    system=os.path.join(os.environ['SystemRoot'],'System32')
    while queue:
        name=queue.pop(0);low=name.lower()
        if low in done:continue
        done.add(low);p=all_local.get(low)
        if p is None:
            if os.path.isfile(os.path.join(system,name)) or low.startswith(('api-ms-','ext-ms-')):continue
            candidates=entries.get(low,[])
            candidates=[e for e in candidates if any(v in e.filename.lower() for v in ['/bind/','/debug/'])==(args.configuration=='Debug')] or candidates
            if len(candidates)!=1: raise RuntimeError('Ambiguous/missing dependency '+name+': '+str([e.filename for e in candidates]))
            e=candidates[0];remote.seek(e.header_offset);header=remote.read(30)
            fields=struct.unpack('<IHHHHHIIIHH',header)
            if fields[0]!=0x04034b50:raise RuntimeError('Invalid local ZIP header')
            remote.seek(e.header_offset+30+fields[-2]+fields[-1]);compressed=remote.read(e.compress_size)
            data=compressed if e.compress_type==0 else zlib.decompress(compressed,-15) if e.compress_type==8 else None
            if data is None or len(data)!=e.file_size or zlib.crc32(data)&0xffffffff!=e.CRC:raise RuntimeError('ZIP CRC mismatch '+e.filename)
            p=os.path.join(bindir,name);open(p,'wb').write(data);all_local[low]=p
            manifest.append({'name':name,'officialAssetId':asset,'member':e.filename,'sha256':hashlib.sha256(data).hexdigest()})
            print('Installed',name,len(data),flush=True)
        queue.extend(imports(p))
    # Also attest matching dependencies installed by a previous interrupted run.
    for low in done:
        p=all_local.get(low)
        if not p or low not in entries:continue
        b=open(p,'rb').read()
        choices=[e for e in entries[low] if len(b)==e.file_size and zlib.crc32(b)&0xffffffff==e.CRC]
        if choices:
            e=choices[0];manifest.append({'name':os.path.basename(p),'officialAssetId':asset,'member':e.filename,'sha256':hashlib.sha256(b).hexdigest()})
    record=os.path.join(sdk,'runtime-'+args.configuration+'.json')
    previous=json.load(open(record,encoding='utf-8')) if os.path.isfile(record) else []
    byname={x['name'].lower():x for x in previous+manifest}
    json.dump(list(byname.values()),open(record,'w',encoding='utf-8',newline=''),indent=2)
    json.dump(sorted(done),open(os.path.join(sdk,'runtime-closure-'+args.configuration+'.json'),'w',encoding='utf-8',newline=''),indent=2)
    print('Runtime dependency closure ready:',len(done),flush=True)
if __name__=='__main__':main()
