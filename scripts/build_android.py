"""Build the native Android APK using official SDK tools (no Gradle/network step).

Requires Android SDK Platform 35, Build Tools 35.0.0 and JDK 17 or newer.
The signing key/password must be supplied from outside source control.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk-root',type=Path,required=True)
    p.add_argument('--java-home',type=Path,required=True)
    p.add_argument('--keystore',type=Path,required=True)
    p.add_argument('--storepass-file',type=Path,required=True)
    p.add_argument('--alias',default='tinglan')
    p.add_argument('--out',type=Path)
    a=p.parse_args()
    sdk=a.sdk_root.resolve(); java=a.java_home.resolve()
    executable='.exe' if os.name=='nt' else ''
    platforms=list(sdk.rglob('android.jar')); tools=list(sdk.rglob('aapt2'+executable))
    if not platforms or not tools: raise SystemExit('Android platform/build tools missing')
    platform=next((f for f in platforms if '35' in str(f)),platforms[0])
    buildtools=next((f.parent for f in tools if '35' in str(f)),tools[0].parent)
    source=ROOT/'mobile/android'
    android_ns='{http://schemas.android.com/apk/res/android}'
    version=ET.parse(source/'AndroidManifest.xml').getroot().attrib[android_ns+'versionName']
    if a.out is None: a.out=ROOT/f'dist/android/Tinglan-Android-{version}.apk'
    stage=ROOT/'build-package/android'
    stage.mkdir(parents=True,exist_ok=True)
    classes=stage/'classes'; classes.mkdir(exist_ok=True)
    generated=stage/'generated'; generated.mkdir(exist_ok=True)
    dex=stage/'dex'; dex.mkdir(exist_ok=True)
    # Stage only reviewed resource definitions and the two original public artworks.
    # Never copy payment images, connection profiles or application data.
    resources=stage/'resources'
    shutil.copytree(source/'res',resources,dirs_exist_ok=True)
    for original,target in [('yvonne-social.png','yvonne_social.png'),('mist-river-v3.png','mist_river.png')]:
        shutil.copy2(ROOT/'assets/art'/original,resources/'drawable'/target)
    env={k.upper():v for k,v in os.environ.items()}
    env['JAVA_HOME']=str(java);env['PATH']=str(java/'bin')+os.pathsep+env.get('PATH','')
    env['TINGLAN_SIGNING_PASSWORD']=a.storepass_file.read_text(encoding='utf-8').strip()
    if len(env['TINGLAN_SIGNING_PASSWORD'])<8: raise SystemExit('Signing password is too short')
    log=stage/'build.log'
    def java_tool(name):return java/'bin'/(name+executable)
    with log.open('w',encoding='utf-8') as stream:
        def run(args,timeout=180):
            command=[]
            for x in args:
                if isinstance(x,Path):
                    try: x=x.resolve().relative_to(ROOT).as_posix()
                    except ValueError: pass
                command.append(str(x))
            subprocess.run(command,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=timeout,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        run([buildtools/('aapt2'+executable),'compile','--dir',resources,'-o',stage/'resources.zip'])
        run([buildtools/('aapt2'+executable),'link','-o',stage/'resources.apk','--manifest',source/'AndroidManifest.xml','-I',platform,'--java',generated,'--auto-add-overlay',stage/'resources.zip'])
        files=sorted((source/'src').rglob('*.java'))+sorted(generated.rglob('*.java'))
        args=stage/'javac.args'
        args.write_text('\n'.join('"'+f.relative_to(ROOT).as_posix()+'"' for f in files),encoding='utf-8')
        run([java_tool('javac'),'-J-Dfile.encoding=UTF-8','-encoding','UTF-8','--release','8','-classpath',platform,'-d',classes,'@'+args.relative_to(ROOT).as_posix()])
        jar=stage/'classes.jar'
        with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as z:
            for f in sorted(classes.rglob('*.class')): z.write(f,f.relative_to(classes))
        run([java_tool('java'),'-cp',buildtools/'lib/d8.jar','com.android.tools.r8.D8','--release','--min-api','26','--lib',platform,'--output',dex,jar])
        unsigned=stage/'unsigned.apk'
        shutil.copy2(stage/'resources.apk',unsigned)
        with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as z:
            for f in dex.glob('*.dex'): z.write(f,f.name)
        run([buildtools/('zipalign'+executable),'-f','-p','4',unsigned,stage/'aligned.apk'])
        a.out.parent.mkdir(parents=True,exist_ok=True)
        signer=buildtools/'lib/apksigner.jar'
        run([java_tool('java'),'-jar',signer,'sign','--ks',a.keystore,'--ks-key-alias',a.alias,'--ks-pass','env:TINGLAN_SIGNING_PASSWORD','--key-pass','env:TINGLAN_SIGNING_PASSWORD','--out',a.out,stage/'aligned.apk'])
        run([java_tool('java'),'-jar',signer,'verify','--verbose','--print-certs',a.out])
        run([buildtools/('aapt2'+executable),'dump','badging',a.out])
    digest=hashlib.sha256(a.out.read_bytes()).hexdigest()
    (a.out.parent/'SHA256SUMS.txt').write_text(digest+'  '+a.out.name+'\n',encoding='utf-8')
    print(json.dumps(dict(apk=str(a.out),bytes=a.out.stat().st_size,sha256=digest,signatureVerified=True,minAndroid='8.0',targetSdk=35),ensure_ascii=False,indent=2))

if __name__=='__main__':main()
