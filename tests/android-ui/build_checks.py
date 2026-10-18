"""Build a separate instrumentation APK; test fixtures are never packaged in the app."""
from pathlib import Path
import argparse,os,subprocess,zipfile,shutil

def main():
    root=Path(__file__).resolve().parents[2]
    parser=argparse.ArgumentParser();parser.add_argument('--sdk-root',type=Path,required=True);parser.add_argument('--java-home',type=Path,required=True);parser.add_argument('--keystore',type=Path,required=True);parser.add_argument('--storepass-file',type=Path,required=True);a=parser.parse_args()
    suffix='.exe' if os.name=='nt' else '';sdk=a.sdk_root.resolve();java=a.java_home.resolve();tools=next(p.parent for p in sdk.rglob('aapt2'+suffix) if '35' in str(p));platform=next(p for p in sdk.rglob('android.jar') if '35' in str(p));stage=root/'build-package/android-ui';stage.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env['UI_SIGN_PASSWORD']=a.storepass_file.read_text().strip()
    def run(args):
        command=[]
        for value in args:
            if isinstance(value,Path):
                try:value=value.resolve().relative_to(root).as_posix()
                except ValueError:pass
            command.append(str(value))
        subprocess.run(command,cwd=root,env=env,check=True,timeout=180,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    classes=stage/'classes';classes.mkdir(exist_ok=True);dex=stage/'dex';dex.mkdir(exist_ok=True)
    run([tools/('aapt2'+suffix),'link','-o',stage/'resources.apk','--manifest',root/'tests/android-ui/AndroidManifest.xml','-I',platform])
    run([java/'bin'/('javac'+suffix),'-encoding','UTF-8','--release','8','-cp',platform,'-d',classes,root/'tests/android-ui/UiChecks.java'])
    with zipfile.ZipFile(stage/'checks.jar','w',zipfile.ZIP_DEFLATED) as z:
        for file in classes.rglob('*.class'):z.write(file,file.relative_to(classes))
    run([java/'bin'/('java'+suffix),'-cp',tools/'lib/d8.jar','com.android.tools.r8.D8','--min-api','26','--lib',platform,'--output',dex,stage/'checks.jar'])
    shutil.copy2(stage/'resources.apk',stage/'unsigned.apk')
    with zipfile.ZipFile(stage/'unsigned.apk','a',zipfile.ZIP_DEFLATED) as z:
        for file in dex.glob('*.dex'):z.write(file,file.name)
    run([tools/('zipalign'+suffix),'-f','4',stage/'unsigned.apk',stage/'aligned.apk'])
    run([java/'bin'/('java'+suffix),'-jar',tools/'lib/apksigner.jar','sign','--ks',a.keystore,'--ks-key-alias','tinglan','--ks-pass','env:UI_SIGN_PASSWORD','--out',stage/'ui-checks.apk',stage/'aligned.apk'])
    print(stage/'ui-checks.apk')
if __name__=='__main__':main()
