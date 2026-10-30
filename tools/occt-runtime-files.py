"""Return the validated PE dependency closure for the OCCT import toolkits."""
import os,runpy,sys
imports=runpy.run_path(os.path.join(os.path.dirname(__file__),'fetch-occt-sdk-runtime.py'))['imports']
folder=os.path.abspath(sys.argv[1])
queue=['TKDESTEP.dll','TKDEIGES.dll','TKMesh.dll','TKXCAF.dll','TKPrim.dll']
done=set();paths=[]
system=os.path.join(os.environ['SystemRoot'],'System32')
while queue:
    name=queue.pop(0);key=name.lower()
    if key in done:continue
    done.add(key);p=os.path.join(folder,name)
    if not os.path.isfile(p):
        if os.path.isfile(os.path.join(system,name)) or key.startswith(('api-ms-','ext-ms-')):continue
        raise RuntimeError('OCCT runtime dependency missing: '+p+'. Complete the SDK runtime before configuring.')
    paths.append(p.replace('\\','/'));queue.extend(imports(p))
if any(os.path.basename(p).lower() in ['tkopengl.dll','tkdraw.dll','tkqadraw.dll'] for p in paths):
    raise RuntimeError('Unexpected OCCT viewer/DRAW dependency')
print(';'.join(paths))
