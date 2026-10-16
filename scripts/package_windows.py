"""Build clean runtime bundle, native single-file launcher and per-user installer.
Never reads bin/data or server/data; only explicit Release binaries and SDK files.
"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[1]
QT=ROOT/'.tools/Qt/6.8.3/msvc2022_64'
CMAKE=ROOT/'.tools/python/cmake/data/bin/cmake.exe'
VERSION=(ROOT/'VERSION').read_text().strip()
OUT=ROOT/f'dist/v{VERSION}'
BUNDLE=ROOT/f'build-package/v{VERSION}/runtime'
def run(args):
    subprocess.run([str(a) for a in args],cwd=ROOT,check=True)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    BUNDLE.mkdir(parents=True,exist_ok=True)
    # Incremental use is safe only for a previously generated clean bundle.
    forbidden={'data','server','tests','.tools','src','gui'}
    if any((BUNDLE/x).exists() for x in forbidden):raise RuntimeError('Unexpected data in release staging directory')
    for source,name in [(ROOT/'build-gui/app/im_desktop.exe','im_desktop.exe'),(ROOT/'build-gui/server/Release/im_server.exe','im_server.exe')]:shutil.copy2(source,BUNDLE/name)
    run([QT/'bin/windeployqt.exe','--release','--no-translations','--no-quick-import','--no-opengl-sw','--skip-plugin-types','generic,designer,help','--exclude-plugins','qsqlmimer,qsqlodbc,qsqlpsql,qdirect2d,qminimal,qopensslbackend','--compiler-runtime','--dir',BUNDLE,BUNDLE/'im_desktop.exe',BUNDLE/'im_server.exe'])
    # Explicit MSVC runtime DLLs, so the portable app needs no prerequisite installer.
    redist=Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC')
    crts=sorted(redist.glob('*/x64/Microsoft.VC*.CRT'))
    if not crts:raise RuntimeError('MSVC CRT not found')
    for dll in crts[-1].glob('*.dll'):shutil.copy2(dll,BUNDLE/dll.name)
    (BUNDLE/'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n',encoding='utf-8')
    (BUNDLE/'packaged.json').write_text(json.dumps({'version':VERSION}),encoding='utf-8')
    (BUNDLE/'client.json').write_text(json.dumps(dict(host='127.0.0.1',port=56110,tls=False,allowInsecureLoopback=True,autoStartLocalServer=True),indent=2),encoding='utf-8')
    shutil.copy2(ROOT/'docs/听澜5使用说明.md',BUNDLE/'使用说明.md')
    shutil.copy2(ROOT/'docs/管理员使用说明.md',BUNDLE/'管理员使用说明.md')
    shutil.copy2(ROOT/'docs/双电脑局域网-v5.3.1.md',BUNDLE/'双电脑局域网说明.md')
    shutil.copy2(ROOT/'docs/自动更新.md',BUNDLE/'自动更新说明.md')
    shutil.copy2(ROOT/'docs/文件传输.md',BUNDLE/'文件传输说明.md')
    licenses=BUNDLE/'licenses';licenses.mkdir(exist_ok=True)
    for sbom in (QT/'sbom').glob('*.spdx.json'):
        if sbom.name.startswith(('qtbase-','qtmultimedia-')):shutil.copy2(sbom,licenses/sbom.name)
    for name in ('LGPLv3','GPLv3','LGPLv21'):
        src=ROOT/f'.tools/Qt/Tools/QtCreator/share/qtcreator/generic-highlighter/syntax/licenses/LICENSE.{name}'
        if src.exists():shutil.copy2(src,licenses/src.name)
    (licenses/'NOTICE.txt').write_text('Qt 6.8.3 (The Qt Company and contributors), dynamically linked.\nLicense terms and third-party notices are included in the SPDX files and license texts.\nQt corresponding source: https://download.qt.io/archive/qt/6.8/6.8.3/submodules/\nFFmpeg binaries supplied by this Qt SDK; their notices and source information are in qtmultimedia SPDX.\nMSVC runtime: Microsoft Corporation, redistributed with this application.\nYou may replace the dynamically linked Qt libraries with compatible modified versions; reverse engineering for debugging those modifications is permitted.\nThe portable launcher extracts these replaceable files to %LOCALAPPDATA%/Tinglan/Runtime/<version>.\n',encoding='utf-8')
    # Only runtime content is archived. Debug symbols and prerequisite installers are unnecessary.
    allowed=[]
    for f in sorted(BUNDLE.rglob('*')):
        if not f.is_file():continue
        if f.suffix.lower() in ('.pdb','.lib','.exp') or f.name.lower().startswith('vc_redist'):continue
        allowed.append(f)
    # Remove only explicitly identified generated deployment extras, inside this exact staging root.
    for f in list(BUNDLE.rglob('*')):
        if f.is_file() and f not in allowed:
            f.resolve().relative_to(BUNDLE.resolve());f.unlink()
    manifest={str(f.relative_to(BUNDLE)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in allowed}
    (OUT/'runtime-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    payload=ROOT/f'build-package/v{VERSION}/runtime.zip'
    with zipfile.ZipFile(payload,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for f in allowed:z.write(f,f.relative_to(BUNDLE))
    digest=hashlib.sha256(payload.read_bytes()).hexdigest()[:20]
    build=ROOT/f'build-portable-v{VERSION}'
    run([CMAKE,'-S',ROOT/'packaging','-B',build,'-G','Visual Studio 18 2026','-A','x64',f'-DPAYLOAD={payload.as_posix()}',f'-DPAYLOAD_HASH={digest}'])
    run([CMAKE,'--build',build,'--config','Release','--parallel','4','--','/nodeReuse:false'])
    shutil.copy2(build/'Release/TinglanPortable.exe',OUT/f'Tinglan-{VERSION}-Portable.exe')
    run([ROOT/'.tools/packaging/InnoSetup/ISCC.exe',f'/DVersion={VERSION}',f'/DBundle={BUNDLE}',f'/DOutput={OUT}',ROOT/'packaging/setup.iss'])
    # Ordinary client is a separately compiled program with no admin windows or local server.
    client=ROOT/f'build-package/v{VERSION}/client-runtime'
    client.mkdir(parents=True,exist_ok=True)
    if any((client/x).exists() for x in ('data','server','.tools','users.json')):raise RuntimeError('Private data found in client staging')
    for f in BUNDLE.iterdir():
        if f.name in ('im_desktop.exe','im_server.exe','client.json','管理员使用说明.md','使用说明.md','双电脑局域网说明.md'):continue
        if f.is_dir():shutil.copytree(f,client/f.name,dirs_exist_ok=True)
        else:shutil.copy2(f,client/f.name)
    shutil.copy2(ROOT/'build-gui/app/im_client.exe',client/'im_client.exe')
    (client/'client.json').write_text(json.dumps(dict(host='',port=56110,tls=True,allowInsecureLoopback=False,autoStartLocalServer=False),indent=2),encoding='utf-8')
    (client/'packaged.json').write_text(json.dumps(dict(version=VERSION,edition='client')),encoding='utf-8')
    shutil.copy2(ROOT/'docs/普通客户端安装与连接码.md',client/'使用说明.md')
    manifest={str(f.relative_to(client)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in client.rglob('*') if f.is_file()}
    (OUT/'client-runtime-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    run([ROOT/'.tools/packaging/InnoSetup/ISCC.exe',f'/DVersion={VERSION}',f'/DBundle={client}',f'/DOutput={OUT}',ROOT/'packaging/client_setup.iss'])
    installer=OUT/f'Tinglan-{VERSION}-Client-Setup.exe'
    (OUT/'client-update.json').write_text(json.dumps(dict(schema=1,edition='client',platform='windows-x64',version=VERSION,
        size=installer.stat().st_size,sha256=hashlib.sha256(installer.read_bytes()).hexdigest(),
        notes=(ROOT/'packaging/update-notes.txt').read_text(encoding='utf-8').strip()),ensure_ascii=False,indent=2),encoding='utf-8')
    for name in (f'Tinglan-{VERSION}-Portable.exe',f'Tinglan-{VERSION}-Setup.exe',f'Tinglan-{VERSION}-Client-Setup.exe'):
        f=OUT/name;print(name,f.stat().st_size,hashlib.sha256(f.read_bytes()).hexdigest())
    (OUT/'SHA256SUMS.txt').write_text('\n'.join(hashlib.sha256((OUT/n).read_bytes()).hexdigest()+'  '+n for n in (f'Tinglan-{VERSION}-Portable.exe',f'Tinglan-{VERSION}-Setup.exe',f'Tinglan-{VERSION}-Client-Setup.exe'))+'\n')
if __name__=='__main__':main()
