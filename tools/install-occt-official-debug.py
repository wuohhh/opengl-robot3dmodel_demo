"""Install only required Debug OCCT binaries/import libraries from the pinned official SDK."""
import hashlib,json,os,runpy,struct,sys,zipfile,zlib
api=runpy.run_path(os.path.join(os.path.dirname(__file__),'fetch-occt-sdk-runtime.py'))
sdk=os.path.abspath(sys.argv[1]);asset,size=api['ASSETS']['debug']
remote=api['RemoteZip'](asset,size);z=zipfile.ZipFile(remote)
queue=['TKDESTEP.dll','TKDEIGES.dll','TKMesh.dll','TKXCAF.dll','TKPrim.dll'];needed=set()
while queue:
    name=queue.pop(0)
    if name.lower() in needed:continue
    needed.add(name.lower())
    p=os.path.join(sdk,'win64','vc14','bin',name)
    if os.path.isfile(p):queue.extend(x for x in api['imports'](p) if x.lower().startswith('tk'))
manifest=[]
for e in z.infolist():
    low=e.filename.lower();name=os.path.basename(low);relative=None
    if '/bind/' in low and name in needed:relative='win64/vc14/bind/'+os.path.basename(e.filename)
    elif '/libd/' in low and name.endswith('.lib') and name[:-4]+'.dll' in needed:relative='win64/vc14/libd/'+os.path.basename(e.filename)
    if relative is None:continue
    target=os.path.abspath(os.path.join(sdk,relative))
    if os.path.commonpath([sdk,target])!=sdk:raise ValueError('Unsafe SDK entry')
    data=open(target,'rb').read() if os.path.isfile(target) else b''
    if len(data)!=e.file_size or zlib.crc32(data)&0xffffffff!=e.CRC:
        remote.seek(e.header_offset);h=struct.unpack('<IHHHHHIIIHH',remote.read(30))
        if h[0]!=0x04034b50:raise ValueError('Invalid ZIP local header')
        remote.seek(e.header_offset+30+h[-2]+h[-1]);b=remote.read(e.compress_size)
        data=b if e.compress_type==0 else zlib.decompress(b,-15) if e.compress_type==8 else None
        if data is None or len(data)!=e.file_size or zlib.crc32(data)&0xffffffff!=e.CRC:raise ValueError('ZIP CRC mismatch')
        os.makedirs(os.path.dirname(target),exist_ok=True);open(target,'wb').write(data)
        assert open(target,'rb').read()==data
    manifest.append({'member':e.filename,'officialAssetId':asset,'sha256':hashlib.sha256(data).hexdigest()})
    print('Debug SDK',relative,len(data),flush=True)
json.dump(manifest,open(os.path.join(sdk,'debug-sdk-manifest.json'),'w',encoding='utf-8',newline=''),indent=2)
record=os.path.join(os.path.dirname(sdk),'..','occt-sdk-Debug.json')
json.dump({'version':'7.9.1','sourceCommit':'36c80dbe7b199348e2f0a2d6932a462a37fd0b90','provenance':'official-prebuilt-vc14-debug','configuration':'Debug','officialAssetId':asset,'occtDir':sdk+'/cmake','prefix':sdk},open(record,'w',encoding='utf-8',newline=''),indent=2)
print('Debug OCCT SDK installed. Complete third-party Debug runtime, then run configure-occt-official-sdk.py --configuration Debug.',flush=True)
