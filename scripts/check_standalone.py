"""Verify deployed Qt and media plugins without the development SDK on PATH."""
from pathlib import Path
import os
import subprocess
import time
import json
root=Path(__file__).resolve().parents[1]
output=root/'tests/gui-output'/f'standalone-{time.time_ns()}'
env={key.upper():value for key,value in os.environ.items() if not key.upper().startswith('QT_')}
env['PATH']=os.pathsep.join([env['SYSTEMROOT'],str(Path(env['SYSTEMROOT'])/'System32')])
result=subprocess.run([str(root/'bin/im_desktop.exe'),'--self-test',str(output)],env=env,cwd=output.parent,timeout=60)
report=json.loads((output/'results.json').read_text(encoding='utf-8'))
if result.returncode or report['status']!='passed':raise SystemExit(report)
print(f"Standalone GUI / music passed: {report['count']} checks")
