"""Export the reviewed Git index for connector upload; never reads ignored files."""
from pathlib import Path
import base64
import hashlib
import json
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'recovery/github-preparation'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def main():
    records=git('ls-files','--stage','-z').split(b'\0')
    text, binary, manifest = [], [], []
    blocked_roots=('bin/', '.tools/', 'build', 'dist/', 'recovery/', 'lan-profiles/', 'server/data/')
    blocked_suffixes=('.key','.pem','.pfx','.p12','.sqlite','.db','.exe','.dll','.apk','.aab','.jks','.keystore','.bak','.log','.user')
    # Detect actual PEM key bodies; header-only negative test fixtures contain no key.
    token_pattern=re.compile(rb'(?:ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----[\r\n]+[A-Za-z0-9+/=]{20,})')
    for record in records:
        if not record:continue
        metadata,rawpath=record.split(b'\t',1);mode,sha,stage=metadata.decode('ascii').split()
        path=rawpath.decode('utf-8')
        if mode!='100644' or stage!='0':raise ValueError('Unexpected Git index entry: '+path)
        if path.startswith(blocked_roots) or path.lower().endswith(blocked_suffixes) or 'output/' in path or path=='assets/payment/wechat-receive.jpg':
            raise ValueError('Private/generated file staged: '+path)
        data=git('cat-file','blob',sha)
        if token_pattern.search(data):raise ValueError('Possible credential staged: '+path)
        item=dict(path=path,mode=mode,type='blob',sha=sha,size=len(data),sha256=hashlib.sha256(data).hexdigest())
        manifest.append(item)
        if path.endswith(('.png','.jpg','.wav')):
            binary.append({**item,'content':base64.b64encode(data).decode('ascii')})
        else:
            text.append(dict(path=path,mode=mode,type='blob',content=data.decode('utf-8')))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'upload-payload.json').write_text(json.dumps(dict(text=text,binary=binary),ensure_ascii=False),encoding='utf-8')
    (OUT/'upload-manifest.json').write_text(json.dumps(dict(fileCount=len(manifest),files=manifest),ensure_ascii=False,indent=2),encoding='utf-8')
    # The archive is also a reproducible clean source snapshot for local clone-build validation.
    with zipfile.ZipFile(OUT/'tinglan-source-reviewed.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for item in manifest:archive.writestr(item['path'],git('cat-file','blob',item['sha']))
    print(json.dumps(dict(files=len(manifest),textFiles=len(text),binaryFiles=len(binary),
        bytes=sum(i['size'] for i in manifest),manifest=str(OUT/'upload-manifest.json')),ensure_ascii=False))


if __name__=='__main__':main()
