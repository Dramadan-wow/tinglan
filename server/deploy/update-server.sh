#!/usr/bin/env bash
# Update an installed Linux central server from an uploaded release bundle.
#
# Run as root from the bundle directory:  bash update-server.sh
# The bundle holds tinglan-<version>-server-source.tar.gz, SHA256SUMS and, optionally,
# Tinglan-<version>-Client-Setup.exe with client-update.json for the client update feed.
# Builds before stopping anything, backs up the data directory, binary and configuration,
# replaces only the binary and rolls it back if the service does not come up.
# server.json, certificates and the database are never rewritten.
set -euo pipefail

SERVICE=${SERVICE:-tinglan}
QT_ROOT=${QT_ROOT:-/opt/Qt/6.8.3/gcc_64}
BIN=${BIN:-/opt/tinglan/im_server}
CONFIG=${CONFIG:-/etc/tinglan/server.json}
DATA=${DATA:-/var/lib/tinglan}
JOBS=${JOBS:-2}

HERE=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=/root/tinglan-backup-$STAMP
step(){ echo; echo "== $*"; }
fail(){ echo "失败：$*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || fail "请用 root 运行"
cd "$HERE"
step "核对上传文件"
sha256sum -c SHA256SUMS || fail "文件不完整，请重新上传"
SOURCE=$(ls tinglan-*-server-source.tar.gz | head -1)
VERSION=${SOURCE#tinglan-}; VERSION=${VERSION%-server-source.tar.gz}
echo "目标版本 $VERSION"
export LD_LIBRARY_PATH="$QT_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
echo "当前版本 $("$BIN" --version 2>&1 | head -1)"

step "编译（服务仍在运行）"
rm -rf src build && mkdir src
tar -xzf "$SOURCE" -C src
[ "$(head -1 src/VERSION)" = "$VERSION" ] || fail "源码 VERSION 与文件名不一致"
cmake -S src/server -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" > build-configure.log
cmake --build build --parallel "$JOBS" > build.log 2>&1 || { tail -30 build.log; fail "编译失败，服务未改动"; }
NEW=$(build/im_server --version 2>&1 | head -1)
[ "$NEW" = "tinglan-server $VERSION" ] || fail "新程序版本异常：$NEW"
echo "编译完成：$NEW"

step "停止服务并备份到 $BACKUP"
systemctl stop "$SERVICE"
trap 'echo "出错，重新启动服务" >&2; systemctl start "$SERVICE"' ERR
mkdir -m 700 "$BACKUP"
cp -a "$DATA" "$BACKUP/data"
cp -a "$BIN" "$BACKUP/im_server"
cp -a "$CONFIG" "$BACKUP/server.json"
du -sh "$BACKUP"

step "替换程序并启动"
install -o root -g root -m 755 build/im_server "$BIN.new"
mv -f "$BIN.new" "$BIN"
systemctl start "$SERVICE"
sleep 3
if ! systemctl is-active --quiet "$SERVICE"; then
    journalctl -u "$SERVICE" -n 30 --no-pager
    echo "新版本未能启动，正在恢复旧程序"
    install -o root -g root -m 755 "$BACKUP/im_server" "$BIN"
    systemctl start "$SERVICE"
    fail "已恢复旧程序，数据备份在 $BACKUP"
fi
trap - ERR
echo "服务运行中：$("$BIN" --version 2>&1 | head -1)"
ss -ltn | grep -E ':5611[01] ' || true

if [ -f client-update.json ]; then
    step "发布普通客户端更新"
    RELEASE=$(mktemp -d /tmp/tinglan-release.XXXXXX)
    cp client-update.json "Tinglan-$VERSION-Client-Setup.exe" "$RELEASE/"
    cp src/scripts/publish_client_update.py "$RELEASE/"
    chown -R "$SERVICE:$SERVICE" "$RELEASE"
    # Publish as the service user so the feed stays readable by the server only.
    (cd "$RELEASE" && runuser -u "$SERVICE" -- python3 "$RELEASE/publish_client_update.py" --release-directory "$RELEASE" --server-config "$CONFIG")
    rm -rf "$RELEASE"
    ls -la "$DATA/updates"
fi

step "完成"
journalctl -u "$SERVICE" -n 5 --no-pager
echo "备份：$BACKUP（确认运行正常后可自行删除）"
