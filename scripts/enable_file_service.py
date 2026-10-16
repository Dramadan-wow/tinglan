"""Enable HTTPS files on an explicitly selected existing TLS workspace, with backups."""
from pathlib import Path
import argparse,json,os,time
from prepare_lan import backup,read_json,resolve_path,write_json

def enable(config,port=56111):
    config=config.resolve();settings=read_json(config)
    if not settings.get('tls') or not settings.get('workspaceFile'):
        raise ValueError('Select an existing TLS central server configuration; plaintext local mode is unchanged')
    if not 1<=port<=65535 or port==settings.get('port'):
        raise ValueError('HTTPS port must be valid and different from the chat port')
    for field in ('workspaceFile','certificateFile','keyFile'):
        if not resolve_path(config,settings[field]).is_file():raise ValueError('Required server file missing: '+field)
    destination=config.parent/'file-service-backups'/str(time.time_ns())
    settings,_,_=backup(config,destination)
    settings.update(fileServiceEnabled=True,filePort=port)
    for key,value in dict(fileStorageDirectory='storage',maxFileMiB=2048,fileQuotaMiB=20480,fileUserQuotaMiB=5120,fileMaxConnections=32,fileTempHours=168).items():settings.setdefault(key,value)
    # Preserve the exact existing database paths, listener identity, certificates and accounts.
    temporary=config.with_name(config.name+'.file-update.tmp')
    try:write_json(temporary,settings);os.replace(temporary,config)
    finally:temporary.unlink(missing_ok=True)
    print('Enabled HTTPS file port:',port);print('Backup:',destination);print('Restart the central server to apply. Existing user database was not modified.')
    return destination
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server-config',required=True,type=Path);p.add_argument('--port',default=56111,type=int);a=p.parse_args();enable(a.server_config,a.port)
