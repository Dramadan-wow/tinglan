"""Validate real console admin menus against isolated data, including restart."""
from pathlib import Path
import os
import subprocess
import sys
import time
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[1]
output=root/'tests/output'/f'admin-{time.time_ns()}'
output.mkdir(parents=True)
file=output/'state.txt'
env={key.upper():value for key,value in os.environ.items()}
checks=0
def expect(ok,name):
    global checks
    checks+=1
    if not ok:raise AssertionError(name)
def run(lines):
    result=subprocess.run([str(root/'bin/im_system.exe'),'--data',str(file)],input='\n'.join(map(str,lines))+'\n',text=True,encoding='utf-8',capture_output=True,env=env,cwd=output,timeout=20)
    expect(result.returncode==0,'console process completes')
    return result.stdout
out=run([5,'10001','123456',0])
expect('管理员账号或密码不正确' in out,'ordinary credential rejected by actual admin menu')
out=run([5,'admin','admin123456',5,10002,1,'menu-reset-2026','menu-reset-2026',3,10004,1,6,7,'admin123456','menu-admin-2026','menu-admin-2026',0,0])
expect('密码已重置' in out and '已禁止登录' in out and '管理员密码已更新' in out,'actual reset disable and admin password forms')
saved=file.read_text(encoding='utf-8')
expect(saved.startswith('TENCENT 4') and 'STATUS 10004 1' in saved,'admin version and disable status persisted')
expect('menu-admin-2026' not in saved and 'ADMINLOG ' in saved,'administrator credential hashed and audit stored')
out=run([5,'admin','admin123456',0])
expect('管理员账号或密码不正确' in out,'old administrator password rejected after restart')
out=run([1,1,10002,1,'123456',1,0,0])
expect('密码不正确' in out and '登录成功' not in out,'old user password rejected after reset')
out=run([1,1,10004,1,'123456',1,0,0])
expect('已被管理员禁止登录' in out and '登录成功' not in out,'disabled account rejects actual console login')
out=run([1,1,10002,1,'menu-reset-2026',1,0,5,'admin','menu-admin-2026',4,10002,1,6,0,0])
expect('登录成功' in out and '用户已登出' in out,'new user password works and administrator forces logout')
saved=file.read_text(encoding='utf-8')
expect('用户登录失败' in saved and '管理员登出' in saved and '强制用户登出' in saved,'actual user attempts and administrator logout recorded')
out=run([5,'admin','menu-admin-2026',2,10004,0,1,1,10004,1,'123456',1,6,0,0])
expect('已允许该用户登录' in out and '登录成功' in out and '已退出登录' in out,'re-enabled user can login and logout')
print(f'PASS {checks} administrator CLI checks')
