"""Exercise the real console and framed, full-duplex localhost TCP."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import threading
import time

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")
root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "bin/im_system.exe"
output = root / "tests/output" / ("smoke-" + str(time.time_ns()))
output.mkdir(parents=True)
env = {key.upper(): value for key, value in os.environ.items()}
checks = 0

def expect(condition, description):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(description)

def run(file, commands="0\n", *options):
    if isinstance(commands, list):
        commands = "\n".join(map(str, commands)) + "\n"
    return subprocess.run([str(exe), "--data", str(file), *map(str, options)], input=commands,
                          text=True, encoding="utf-8", capture_output=True, env=env, cwd=output, timeout=15)

def login(user=10001, service=1, linked=True):
    lines = [1, 1, user, service, "123456"]
    if linked:
        lines.append(1)
    return lines + [0]

data = output / "中文存档.txt"
r = run(data, ["abc", "0abc", 4, 2, 0, 0])
expect(r.returncode == 0, "console exit")
expect(r.stdout.count("请输入完整整数") == 2, "strict numbers")
expect("用户 4 / 服务 10" in r.stdout and "待处理好友申请 1" in r.stdout, "statistics")
expect(data.read_text(encoding="utf-8").startswith("TENCENT 3"), "Unicode path and version3")
expect(run(data).returncode == 0, "saved restart")
r = run(data, login() + [3, 1, 0, 0, 0])
expect(r.returncode == 0 and "这是一条演示私聊" in r.stdout and "欢迎使用连续群聊" in r.stdout, "unread inbox private and group")
expect("未读 0" in r.stdout, "displayed messages read")

r = run(data, login() + [3, 2, 10002, "连续私聊中文 | \"引号\" \\", "/star 3", "/find 连续私聊", 0, "/back", 5, 0, 0, 0])
expect(r.returncode == 0 and "已发送 #3" in r.stdout, "continuous private send")
expect("[已收藏]" in r.stdout, "favorite shown")
expect('STAR 3 10001' in data.read_text(encoding="utf-8"), "favorite persisted")
r = run(data, login() + [3, 8, 1, 10002, "中文导出.txt", 7, 3, 5, 0, 0])
expect(r.returncode == 0 and "已导出" in r.stdout and "消息已撤回" in r.stdout, "export and recall actions")
expect((output / "exports/中文导出.txt").read_text(encoding="utf-8-sig").find("连续私聊中文") >= 0, "export content")
expect('STAR 3 ' not in data.read_text(encoding="utf-8"), "recall clears favorites")

r = run(data, login() + [2, 2, 4, "临时菜单群", 1, 3, 1008, 4, 8, 1, 0, 0, 0])
expect(r.returncode == 0 and "群组已解散" in r.stdout and "提示：群不存在" not in r.stdout, "disband safely closes submenu")
expect("临时菜单群" not in data.read_text(encoding="utf-8"), "disband stored")
r = run(data, [1, 2, 20001, "/back", 0, 0])
expect(r.returncode == 0 and "已取消" in r.stdout and 'USER 20001 ' not in data.read_text(encoding="utf-8"), "cancel form no partial register")

# Real friend request, recipient approval, then mutual deletion and block.
r = run(data, login(10002) + [2, 1, 3, 1, 6, 1, 0, 0, 0, 0])
expect(r.returncode == 0 and "申请已处理" in r.stdout, "recipient accepts demo request")
saved = data.read_text(encoding="utf-8")
expect('FRIEND 10002 0 10003 3 ' in saved and 'FRIEND 10003 3 10002 0 ' in saved, "mutual cross-service friend persisted")
r = run(data, login(10002) + [2, 1, 8, 10003, 1, 1, 0, 0, 0])
expect(r.returncode == 0 and "黑名单已更新" in r.stdout, "blacklist menu")
saved = data.read_text(encoding="utf-8")
expect('BLOCK 10002 0 10003' in saved and 'FRIEND 10003 3 10002 0 ' not in saved, "blacklist removes reciprocal relationship")

r = run(data, login() + [3, 3, 1001, "/at -1 全体中文消息", "/back", 0, 0])
expect(r.returncode == 0 and "已发送" in r.stdout and "@全体 全体中文消息" in data.read_text(encoding="utf-8"), "group mention command")
r = run(data, login() + [2, 2, 3, 1001, 4, 4, 10002, 30, 0, 0, 0, 0, 0])
expect(r.returncode == 0 and "禁言设置已更新" in r.stdout, "mute management")
r = run(data, login(10002) + [3, 3, 1001, "我被禁言", "/back", 0, 0])
expect(r.returncode == 0 and "你目前被禁言" in r.stdout and "我被禁言" not in data.read_text(encoding="utf-8"), "mute denies actual console send")

# Paging opens the latest page and must not mark earlier hidden pages read.
page_data = output / "分页.txt"
r = run(page_data, login() + [3, 2, 10002] + [f"分页消息{i}" for i in range(12)] + ["/back", 0, 0])
expect(r.returncode == 0, "many messages send")
r = run(page_data, login(10002) + [3, 1, 0, 0, 0])
expect(r.returncode == 0 and "第 2/2 页" in r.stdout and "未读 4" in r.stdout, "only visible latest page read")

broken = output / "损坏存档.txt"
broken.write_text("TENCENT 3\nUSER invalid\n", encoding="utf-8")
before = broken.read_bytes()
r = run(broken)
expect(r.returncode == 1 and "启动失败" in r.stderr and broken.read_bytes() == before, "corrupt startup preserves file")
source = root / "tests/fixtures/legacy.txt"
original = source.read_bytes()
imported = output / "迁移结果.txt"
r = run(imported, "", "--import", source)
expect(r.returncode == 0 and "导入成功" in r.stdout, "legacy import CLI")
expect(source.read_bytes() == original and not Path(str(source) + ".original.bak").exists(), "import source untouched")
expect(imported.read_text(encoding="utf-8").startswith("TENCENT 3"), "import writes new format")
r = run(imported, "", "--import", source)
expect(r.returncode == 1 and "导入目标已存在" in r.stderr, "import refuses existing target")
expect(run(output / "eof.txt", "").returncode == 0, "EOF saves cleanly")

class Process:
    def __init__(self, filename):
        self.process = subprocess.Popen([str(exe), "--data", str(output / filename)], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                        encoding="utf-8", bufsize=1, cwd=output, env=env)
        self.output = ""
        self.changed = threading.Condition()
        self.reader = threading.Thread(target=self.collect, daemon=True)
        self.reader.start()
    def collect(self):
        while True:
            c = self.process.stdout.read(1)
            if not c:
                break
            with self.changed:
                self.output += c
                self.changed.notify_all()
    def send(self, text):
        self.process.stdin.write(text)
        self.process.stdin.flush()
    def wait(self, text):
        deadline = time.monotonic() + 15
        with self.changed:
            while text not in self.output:
                if self.process.poll() is not None or time.monotonic() > deadline:
                    raise AssertionError("Missing " + text + "\n" + self.output[-3000:])
                self.changed.wait(0.1)
    def finish(self):
        code = self.process.wait(timeout=15)
        self.reader.join(2)
        expect(code == 0, "TCP process clean exit")
    def stop(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=5)

def port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

if os.name == "nt":
    number = port(); server = Process("server.txt"); client = None
    try:
        server.send(f"4\n4\n1\n{number}\n"); server.wait("等待连接")
        client = Process("client.txt"); client.send(f"4\n4\n2\n127.0.0.1\n{number}\n")
        client.wait("已连接"); server.wait("已连接")
        client.send("客户端中文消息\n"); server.wait("客户端中文消息")
        server.send("服务端中文消息\n"); client.wait("服务端中文消息")
        expect(True, "full duplex Unicode TCP")
        client.send("/quit\n0\n0\n0\n"); client.finish()
        server.wait("连接已关闭"); server.send("\n0\n0\n0\n"); server.finish()
    finally:
        server.stop()
        if client:
            client.stop()
    server = Process("framing.txt"); peer = None
    try:
        number = port(); server.send(f"4\n4\n1\n{number}\n"); server.wait("等待连接")
        peer = socket.create_connection(("127.0.0.1", number), timeout=5); server.wait("已连接")
        content = ("中文分包" * 200 + "-END").encode("utf-8")
        frame = struct.pack("!I", len(content)) + content
        peer.sendall(frame[:2]); peer.sendall(frame[2:81]); peer.sendall(frame[81:]); server.wait("-END")
        expect(True, "fragmented TCP frame")
        for text in ("第一条", "第二条"):
            content = text.encode("utf-8"); peer.sendall(struct.pack("!I", len(content)) + content)
        server.wait("第二条"); expect("第一条" in server.output, "consecutive TCP frames")
        peer.sendall(struct.pack("!I", 7001)); server.wait("连接已关闭"); expect(True, "oversized frame rejected")
        server.send("\n0\n0\n0\n"); server.finish()
    finally:
        if peer:
            peer.close()
        server.stop()
print(f"PASS {checks} CLI/TCP checks")
