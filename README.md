# 听澜 · C++ / Qt 即时通信系统

当前版本 **5.7.0**。Qt 6.8.3、C++17、CMake，Windows 使用 Qt Widgets 客户端，独立服务端使用 Qt TCP/TLS 与 SQLite。

## 下载安装

原生 **Android 1.3.1** 手机与平板普通客户端，与现有服务器互通。界面与电脑端统一：登录后使用浅蓝工作区配色，会话、好友、群组、朋友圈和通知分区，聊天显示头像与对方选择的气泡样式；平板横屏使用电脑端同款侧边栏与“会话列表 + 聊天”双栏。1.3.0 起支持自动登录、“扫一扫”登录电脑端，以及与好友共用的聊天小火人。安装、功能范围和构建方式见 [安卓客户端说明](docs/安卓客户端.md)。安卓版本独立于桌面 5.7.0；APK 与电脑端安装包一同在发行页提供，不进入源码 Git 历史。

在 [5.7.0 下载页](https://github.com/Dramadan-wow/tinglan/releases/tag/v5.7.0) 下载 Windows 64 位安装包。普通用户选择 **Tinglan-5.7.0-Client-Setup.exe**；首次启动向服务器维护者取得文字连接码，导入后保存，之后会记住服务器。安装包没有预置个人云端地址或账号。

完整管理版和便携版用于维护者，包含管理入口及本机服务端。连接云端管理空间前，也应先导入同一服务器的连接码。详见 [安装与云端连接说明](docs/下载安装与云端连接.md)。仓库保持私有，下载需要具有仓库访问权限的 GitHub 账号。

新增 [私聊文件传输](docs/文件传输.md)：独立 HTTPS 分块上传、Range 下载、暂停/恢复/取消/重试、离线文件、SHA-256 校验和聊天文件卡片。现有 TLS 聊天负责通知，文件内容不进入 JSON 聊天消息。服务端需显式启用文件服务并开放受信任客户端的文件端口。

支持 QQ、微信、微博三个服务视图；普通私聊、好友、群组、通知、账号、好友朋友圈图片、公共微博图片与陌生人私信、装扮、会员申请和管理员审核由同一中心服务器处理。

登录页提供本地、局域网、云服务器连接设置。远程连接强制验证 TLS 证书；服务器地址在部署时填写，不固定为开发电脑当前 IP。两台电脑使用相同后端及数据库。普通客户端单独编译，不包含管理员界面或本机服务；安装后粘贴文字连接码即可连接。头像与会员装扮由中心服务器同步。

## 仓库内容

```text
src/                  共用业务模块与控制台程序
gui/                  Qt Widgets 客户端
network/              帧编码、TCP/TLS、连接和消息协议
workspace/            中心业务数据编码
server/               独立 Qt 服务端和 SQLite 存储
config/               客户端示例配置
assets/               界面、图标、音乐及收款占位资源
packaging/            单文件启动器与 Windows 安装配置
scripts/              构建、打包与局域网部署工具
tests/                隔离数据的核心和网络/GUI测试
docs/                 使用、构建与部署说明
.github/workflows/    Windows 编译测试与 Ubuntu 服务端编译
VERSION               当前版本
CHANGELOG.md          功能更新记录
```

真实用户数据、数据库、证书/私钥、收款二维码、SDK、生成的 EXE 和本机恢复记录不上传。`.gitignore` 不会删除这些本机文件；新电脑从源码构建使用收款占位图。详见 [资源说明](assets/payment/README.md)。

## Windows 构建

安装支持 C++17 的 Visual Studio、CMake，以及 Qt 6.8.3 MSVC 2022 x64（包含 Multimedia）。在开发者 PowerShell 运行：

```powershell
cmake -S . -B build-source -G "Visual Studio 17 2022" -A x64 -DIM_BUILD_GUI=ON -DIM_BUILD_SERVER=ON -DCMAKE_PREFIX_PATH="你的Qt目录" -DIM_DESKTOP_OUTPUT="$PWD/build-source/app"
cmake --build build-source --config Release --parallel 4
ctest --test-dir build-source -C Release --output-on-failure
```

使用 Visual Studio 2026 时把生成器改为 `Visual Studio 18 2026`。当前开发电脑的 `CMakePresets.json` 和 `scripts/build_gui.py` 是本机工具路径入口，新电脑可使用上面的通用命令。

开发运行前用 `windeployqt` 部署 Qt 依赖；运行独立服务端与客户端：

```powershell
.\build-source\server\Release\im_server.exe --config .\server\config\local.example.json
.\build-source\app\im_desktop.exe --client-config .\config\client.local.example.json
```

这里的明文配置仅允许本机 127.0.0.1 开发测试。新服务端没有真实用户，客户端可注册新账号。管理员需维护者按 [服务端说明](docs/中心服务器与双端测试.md) 显式配置，不能把演示凭据当成正式登录系统。

## Ubuntu 独立服务端构建

安装 Qt 6.8 或更高的 Core、Network、Sql、Gui 与 CMake/C++ 编译器：

```sh
cmake -S server -B build-linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR"
cmake --build build-linux --parallel 4
./build-linux/im_server --help
```

实际部署需要填写服务器配置、TLS 证书及数据路径，不能直接使用测试配置开放互联网监听。Ubuntu CI 和云端部署状态以真实执行结果为准。

## 使用与部署

- [听澜使用说明](docs/听澜5使用说明.md)
- [下载安装与云端连接](docs/下载安装与云端连接.md)
- [双电脑局域网、TLS、防火墙及恢复本地模式](docs/双电脑局域网-v5.3.1.md)
- [普通客户端安装、文字连接码与头像气泡同步](docs/普通客户端安装与连接码.md)
- [管理员使用说明](docs/管理员使用说明.md)
- [会员与装扮](docs/会员与装扮使用说明.md)
- [统一平台协议与操作](docs/统一平台使用与验证.md)
- [GitHub、版本发布和自动更新准备](docs/GitHub与自动更新准备.md)

源码与发行包分开管理，Windows 安装包通过 GitHub Releases 分发。普通客户端支持从中心服务器检查、下载和安装升级；维护者需要另行发布服务器更新源，上传 GitHub Release 不会自动配置客户端更新服务。完整管理版及服务端仍由维护者更新。

Qt 等第三方依赖及资源的授权条件须按各自许可证处理；本仓库没有替项目作者擅自授予开源许可证。

普通客户端新增 TLS 自动检查、下载与安装升级，详见 [自动更新说明](docs/自动更新.md)。首次从 5.4 升级须安装一次 5.5 普通版。
