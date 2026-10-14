"""Explicit offline migration. Preview by default; --apply requires exclusive app locks.

Never run automatically at startup. Recognize original demo identities by multiple
fields, preserve renamed accounts, back up every affected file, validate using C++.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / 'build-gui/Release/workspace_data_tool.exe'
DEMO = {
    10001: ('张三', '2000-05-01', '2016-09-01', '长春'),
    10002: ('李四', '1999-11-11', '2017-09-01', '北京'),
    10003: ('王五', '2001-03-03', '2019-09-01', '上海'),
    10004: ('赵六', '2000-12-12', '2018-09-01', '广州'),
}

def transform(original):
    d = copy.deepcopy(original)
    removed = {u['id'] for u in d['users'] if
               tuple(u[k] for k in ('name', 'birth', 'applied', 'location')) == DEMO.get(u['id'])
               and all(s['nick'] == u['name'] for s in u['services'])}
    retired = sum(s['type'] not in (0, 1, 2) for u in d['users'] for s in u['services'])
    d['users'] = [u for u in d['users'] if u['id'] not in removed]
    for u in d['users']:
        u['services'] = [s for s in u['services'] if s['type'] in (0, 1, 2)]
        if not u['services']:
            raise RuntimeError(f"真实账号 {u['id']} 仅有下线服务，需先迁移账号，未执行删除")
    users = {u['id']: u for u in d['users']}
    def valid(uid, service):
        return uid in users and any(s['type'] == service for s in users[uid]['services'])
    for u in d['users']:
        for s in u['services']:
            s['friends'] = [f for f in s['friends'] if valid(f['id'], f['contact'])]
            s['blocked'] = [uid for uid in s['blocked'] if uid in users]
    groups, transfers = [], []
    for g in d['groups']:
        members = []
        for m in g['members']:
            m['services'] = [s for s in m['services'] if valid(m['id'], s)]
            if m['services']: members.append(m)
        if not members: continue
        g['members'] = members
        ids = {m['id'] for m in members}
        if g['owner'] not in ids:
            old = g['owner']; g['owner'] = min(ids)
            transfers.append({'group': g['id'], 'from': old, 'to': g['owner']})
        g['admins'] = [uid for uid in g['admins'] if uid in ids]
        g['mutes'] = [m for m in g['mutes'] if m[0] in ids]
        g['pending'] = [p for p in g['pending'] if valid(p[0], p[1])]
        groups.append(g)
    group_ids = {g['id'] for g in groups}
    d['groups'] = [g for g in groups if not g['parent'] or g['parent'] in group_ids]
    group_ids = {g['id'] for g in d['groups']}
    d['messages'] = [m for m in d['messages'] if m['sender'] in users
                     and (m['target'] in users if m['kind'] == 0 else m['target'] in group_ids)
                     and m['source'] in (0, 1, 2) and m['destination'] in (0, 1, 2)]
    # Exact fixture messages only: never delete arbitrary conversations of renamed users.
    demo_texts = {'这是一条演示私聊，可收藏、导出或查看回执。', '欢迎使用连续群聊，输入/help查看命令。'}
    d['messages'] = [m for m in d['messages'] if not
                     (m['sender'] == 10002 and m['id'] in ('1', '2') and m['text'] in demo_texts)]
    for m in d['messages']:
        for key in ('read', 'stars'): m[key] = [uid for uid in m[key] if uid in users]
    d['requests'] = [r for r in d['requests'] if valid(r['from'], r['source']) and valid(r['to'], r['destination'])]
    d['adminLogs'] = [l for l in d.get('adminLogs', []) if
                      l['target'] not in removed and l['service'] in (0, 1, 2, 7)
                      and l['actor'] not in {f'用户:{uid}' for uid in removed}
                      and l['actor'] not in {f'用户:{DEMO[uid][0]}' for uid in removed}]
    # Credentials, personal details, account status and active service numbers must not change.
    for u in d['users']:
        before = next(v for v in original['users'] if v['id'] == u['id'])
        assert {k:v for k,v in u.items() if k != 'services'} == {k:v for k,v in before.items() if k != 'services'}
        for s in u['services']:
            old = next(v for v in before['services'] if v['type'] == s['type'])
            assert {k:v for k,v in s.items() if k not in ('friends','blocked')} == {k:v for k,v in old.items() if k not in ('friends','blocked')}
    assert d.get('adminHash') == original.get('adminHash')
    report = {'deletedDemoAccounts': [{'id':u['id'],'name':u['name']} for u in original['users'] if u['id'] in removed],
              'keptAccounts': [{'id':u['id'],'name':u['name']} for u in d['users']],
              'retiredServices': retired, 'groupTransfers': transfers,
              'removedMessages':len(original['messages'])-len(d['messages']),
              'credentialsAndProfilesUnchanged':True}
    return d, removed, report

def invoke(*args):
    return subprocess.run([str(TOOL), *map(str,args)], check=True, capture_output=True).stdout

def clean_sql(db, removed):
    tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    for uid in sorted(removed):
        for table, columns in [('posts',['author']),('comments',['author']),('likes',['actor']),
                               ('follows',['actor','target']),('social_blocks',['actor','target']),
                               ('direct_messages',['sender','recipient']),('memberships',['actor']),
                               ('membership_orders',['actor']),('operations',['actor'])]:
            if table in tables: db.execute(f'DELETE FROM {table} WHERE '+ ' OR '.join(f'{c}=?' for c in columns), [uid]*len(columns))
    for table in ('likes','comments'):
        if table in tables: db.execute(f'DELETE FROM {table} WHERE post NOT IN (SELECT id FROM posts)')
    # Preserve a real user's direct-message text even when its referenced demo post is gone.
    if 'direct_messages' in tables: db.execute('UPDATE direct_messages SET post=0 WHERE post NOT IN (SELECT id FROM posts)')

def migrate(file, output, apply=False, keep_profiles=()):
    file = file.resolve(strict=True); output.mkdir(parents=True, exist_ok=False)
    lock = None
    try:
        if apply:
            lock = subprocess.Popen([str(TOOL),'lock',str(file)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if lock.stdout.readline().strip() != b'locked':
                raise RuntimeError(lock.stderr.read().decode('utf-8',errors='replace'))
        sql = file.suffix == '.sqlite'
        if sql:
            with sqlite3.connect(file.as_uri()+'?mode=ro',uri=True) as source, sqlite3.connect(output/'before.sqlite') as backup:
                source.backup(backup)
            db = sqlite3.connect(output/'before.sqlite')
            original = json.loads(db.execute('SELECT payload FROM state WHERE id=1').fetchone()[0]); db.close()
        else:
            shutil.copy2(file,output/'before.txt')
            original = json.loads(invoke('read-text',file))
        result, removed, report = transform(original)
        report.update(file=str(file),applied=apply)
        candidate = output/'after.json';candidate.write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
        invoke('validate',candidate)
        if sql:
            shutil.copy2(output/'before.sqlite',output/'after.sqlite')
            with sqlite3.connect(output/'after.sqlite') as db:
                tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
                member_tables = [t for t in ('membership_orders','memberships') if t in tables]
                protected = {t:db.execute(f'SELECT * FROM {t} WHERE actor NOT IN ({",".join("?" for _ in removed) or "NULL"})',list(removed)).fetchall()
                             for t in member_tables} if removed else {t:db.execute(f'SELECT * FROM {t}').fetchall() for t in member_tables}
                clean_sql(db,removed)
                db.execute('UPDATE state SET payload=? WHERE id=1',(json.dumps(result,ensure_ascii=False).encode('utf-8'),))
                for t, rows in protected.items(): assert db.execute(f'SELECT * FROM {t}').fetchall() == rows
                assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
                report['preservedMembershipOrders'] = len(protected.get('membership_orders',[]))
                report['preservedMemberships'] = len(protected.get('memberships',[]))
            if apply:
                with sqlite3.connect(file) as db:
                    db.execute('BEGIN IMMEDIATE')
                    assert json.loads(db.execute('SELECT payload FROM state WHERE id=1').fetchone()[0]) == original, 'Data changed during maintenance'
                    clean_sql(db,removed)
                    db.execute('UPDATE state SET payload=? WHERE id=1',(json.dumps(result,ensure_ascii=False).encode('utf-8'),))
        elif apply:
            invoke('write-text',candidate,file)
        # Online profiles use server IDs even when a local text cache is still an old demo copy.
        studio = Path(str(file)+'.studio.json')
        if studio.exists():
            shutil.copy2(studio,output/'before.studio.json')
            s=json.loads(studio.read_text(encoding='utf-8'));deletable=removed-set(keep_profiles)
            s['profiles']={k:v for k,v in s['profiles'].items() if int(k) not in deletable}
            s['orders']=[o for o in s['orders'] if o['user'] not in deletable]
            encoded=json.dumps(s,ensure_ascii=False,indent=2)
            (output/'after.studio.json').write_text(encoded,encoding='utf-8')
            if apply:
                temp=studio.with_suffix('.json.cleanup.tmp');temp.write_text(encoded,encoding='utf-8');os.replace(temp,studio)
        (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        return report
    finally:
        if lock:
            if lock.poll() is None: lock.communicate(b'\n',timeout=10)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--file',type=Path,action='append',required=True)
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--keep-profile-ids',default='')
    args=parser.parse_args()
    os.environ['PATH']=str(ROOT/'.tools/Qt/6.8.3/msvc2022_64/bin')+os.pathsep+os.environ['PATH']
    output=ROOT/'recovery'/f'three-platforms-{time.time_ns()}'
    reports=[migrate(f,output/str(i),args.apply,{int(x) for x in args.keep_profile_ids.split(',') if x}) for i,f in enumerate(args.file)]
    print(json.dumps({'backup':str(output),'results':reports},ensure_ascii=False,indent=2))

if __name__=='__main__':main()
