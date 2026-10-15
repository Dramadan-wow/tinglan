"""Publish only an ordinary-client installer to the central server's TLS update feed.

No server credentials, database or configuration is modified. Run on the server owner machine.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile

def version(text):
    if not isinstance(text,str) or not re.fullmatch(r'(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})',text):
        raise ValueError('Invalid release version')
    return tuple(map(int,text.split('.')))

def publish(release_directory, server_config):
    release_directory=Path(release_directory).resolve()
    config_path=Path(server_config).resolve()
    config=json.loads(config_path.read_text(encoding='utf-8-sig'))
    if config.get('tls') is not True:raise ValueError('Updates require TLS; plaintext mode is not supported')
    target=(config_path.parent/config.get('updatesDirectory','updates')).resolve()
    manifest=json.loads((release_directory/'client-update.json').read_text(encoding='utf-8'))
    ver=version(manifest.get('version'))
    if manifest.get('schema')!=1 or manifest.get('edition')!='client' or manifest.get('platform')!='windows-x64':raise ValueError('Only ordinary Windows client packages may be published')
    if not isinstance(manifest.get('notes'),str) or len(manifest['notes'])>4000:raise ValueError('Invalid release notes')
    source=release_directory/f'Tinglan-{manifest["version"]}-Client-Setup.exe'
    size=source.stat().st_size
    if not 1024<=size<=256*1024*1024 or manifest.get('size')!=size:raise ValueError('Invalid installer size')
    with source.open('rb') as f:
        if f.read(2)!=b'MZ':raise ValueError('Not a Windows executable')
    with source.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    if manifest.get('sha256')!=digest:raise ValueError('Installer SHA-256 does not match the build manifest')
    target.mkdir(parents=True,exist_ok=True)
    # Serialize publishers; never replace immutable versioned files with other bytes.
    lock=target/'.publish.lock'
    with lock.open('x',encoding='utf-8') as f:f.write(str(os.getpid()))
    temporary=[]
    try:
        feed=target/'client-stable.json'
        if feed.exists():
            old=json.loads(feed.read_text(encoding='utf-8'))
            if version(old['version'])>ver:raise ValueError('Refusing to downgrade the published release')
            if old['version']==manifest['version'] and old['sha256']!=digest:raise ValueError('Version already published with different content; increase VERSION')
        destination=target/source.name
        if destination.exists():
            with destination.open('rb') as f:existing=hashlib.file_digest(f,'sha256').hexdigest()
            if existing!=digest:raise ValueError('Versioned installer already exists with different content')
        else:
            fd,tmp=tempfile.mkstemp(prefix='.installer-',dir=target);os.close(fd);temporary.append(Path(tmp))
            shutil.copyfile(source,tmp)
            with open(tmp,'rb') as f:
                if hashlib.file_digest(f,'sha256').hexdigest()!=digest:raise ValueError('Copy verification failed')
            os.replace(tmp,destination)
        fd,tmp=tempfile.mkstemp(prefix='.manifest-',dir=target);temporary.append(Path(tmp))
        with os.fdopen(fd,'w',encoding='utf-8') as f:
            json.dump(manifest,f,ensure_ascii=False,indent=2);f.flush();os.fsync(f.fileno())
        os.replace(tmp,feed)
        return feed
    finally:
        for path in temporary:path.unlink(missing_ok=True)
        lock.unlink(missing_ok=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-directory',required=True)
    parser.add_argument('--server-config',required=True)
    args=parser.parse_args()
    print(publish(args.release_directory,args.server_config))

if __name__=='__main__':main()
