#!/usr/bin/env bash
# Publish ordinary-client packages to the running server's update feed without rebuilding it.
#
# Run as root from the bundle directory:  bash publish-update.sh
# The bundle holds publish_client_update.py, SHA256SUMS and one or both of
#   client-update.json + Tinglan-<version>-Client-Setup.exe   (Windows)
#   android-update.json + Tinglan-Android-<version>.apk       (Android)
# Only the update directory changes; the service keeps running.
set -euo pipefail

SERVICE=${SERVICE:-tinglan}
CONFIG=${CONFIG:-/etc/tinglan/server.json}
DATA=${DATA:-/var/lib/tinglan}
fail(){ echo "失败：$*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || fail "请用 root 运行"
cd "$(dirname "$0")"
sha256sum -c SHA256SUMS || fail "文件不完整，请重新上传"
published=0
for manifest in client-update.json android-update.json; do
    [ -f "$manifest" ] || continue
    package=$(python3 -c 'import json,sys;m=json.load(open(sys.argv[1],encoding="utf-8"));print(("Tinglan-Android-%s.apk" if m["platform"]=="android" else "Tinglan-%s-Client-Setup.exe")%m["version"])' "$manifest")
    [ -f "$package" ] || fail "缺少 $package"
    release=$(mktemp -d /tmp/tinglan-release.XXXXXX)
    cp "$manifest" "$package" publish_client_update.py "$release/"
    chown -R "$SERVICE:$SERVICE" "$release"
    (cd "$release" && runuser -u "$SERVICE" -- python3 "$release/publish_client_update.py" --release-directory "$release" --server-config "$CONFIG" --manifest "$manifest")
    rm -rf "$release"
    echo "已发布：$package"
    published=1
done
[ "$published" = 1 ] || fail "没有找到 client-update.json 或 android-update.json"
ls -la "$DATA/updates"
