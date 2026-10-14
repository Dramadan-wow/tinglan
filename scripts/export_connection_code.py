"""Export only public TLS connection material from an existing deployment profile."""
from pathlib import Path
import argparse,base64,hashlib,json
import prepare_lan

def encode(config):
    value=prepare_lan.read_json(config)
    if not value.get('tls') or value.get('autoStartLocalServer') or value.get('allowInsecureLoopback'):
        raise ValueError('Only a verified TLS central-server profile may be shared')
    prepare_lan.identity(value['host'])
    port=value['port']
    if type(port) is not int or not 1<=port<=65535:raise ValueError('Invalid port')
    pem=prepare_lan.resolve_path(config,value['caFile']).read_text(encoding='utf-8') if value.get('caFile') else ''
    if len(pem.encode())>8192 or 'PRIVATE KEY' in pem or 'PUBLIC KEY' in pem or (pem and pem.count('BEGIN CERTIFICATE')!=1):raise ValueError('Only one public CA certificate may be exported')
    if pem:
        from cryptography import x509
        x509.load_pem_x509_certificate(pem.encode())
    mode=value.get('mode','lan')
    if mode not in ('lan','cloud'):raise ValueError('Invalid mode')
    body=json.dumps(dict(version=1,host=value['host'],port=port,ca=pem,mode=mode),separators=(',',':'),ensure_ascii=False).encode()
    return 'TINGLAN1.'+base64.urlsafe_b64encode(body).rstrip(b'=').decode()+'.'+hashlib.sha256(body).hexdigest()[:16]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--client-config',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    code=encode(a.client_config.resolve());a.out.parent.mkdir(parents=True,exist_ok=True)
    # Never overwrite an existing connection code without review.
    with a.out.open('x',encoding='utf-8') as f:f.write(code+'\n')
    print('Public connection code:',a.out)
if __name__=='__main__':main()
