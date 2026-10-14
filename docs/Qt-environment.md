# Qt 环境与桌面界面

## 已配置的环境

| 组件 | 版本 / 位置 |
|---|---|
| Qt SDK | 6.8.3，`.tools/Qt/6.8.3/msvc2022_64` |
| Qt 模块 | Core、Gui、Widgets、Network、Multimedia，以及 Qt Tools / Designer |
| Qt Creator | 20.0.2，`.tools/Qt/Tools/QtCreator/bin/qtcreator.exe` |
| CMake | 4.4.4，`.tools/python/cmake/data/bin/cmake.exe` |
| C++ 编译器 | 现有 Visual Studio 18 Community，MSVC v145，x64 |
| SDKTool | 官方工具，已为项目本地 Creator 注册 Qt 和 CMake |

工具放在工程内，不修改系统 PATH。`进入Qt环境.cmd` 只为打开的终端设置 Qt、CMake 与 MSVC 环境；关闭终端即结束。Qt 的 `qt.conf` 使用相对 Prefix，解决安装器处理中文路径时的编码问题。

## 运行

1. 双击 `启动图形界面.cmd`，输入昵称、选择服务并输入密码。
2. 演示昵称 `张三` / `123456` 使用 QQ；`王五` 使用微信，`李四`、`赵六`也可以使用 QQ。
3. `启动Qt演示.cmd` 使用带示例消息与好友申请的独立演示存档。
4. 左侧切换页面，顶部切换本人服务或账号。
5. 管理员使用 `启动管理员模式.cmd`，或在登录页选择“管理空间”。初始凭据为 `admin` / `admin123456`，进入独立的用户管理后台。

顶部“切换账号 / 登出”或后台“退出管理 / 返回登录”会结束当前身份会话并返回登录页，可切换普通用户与管理员。详见 [管理员使用说明](管理员使用说明.md)。

听澜登录页带原创轻音乐《雾岸》，点击播放开始，支持暂停、进度、音量和选择本地音频；登录成功或关闭登录页会停止播放。音频通过 Qt 资源打包，无需网络。普通用户界面隐藏平台用户ID，注册自动分配编号，好友与邀请通过昵称选择。服务号码与群号仍按课程规则展示。昵称要求唯一，改名后使用新昵称登录。

正式存档为 `bin/data/tencent_data.txt`，命令行和桌面版均可读取。请关闭当前程序后再用另一入口打开同一存档。桌面版有文件锁，阻止两个桌面实例同时打开同一文件。

TCP 演示已升级为中心服务器：先用 `scripts/create_remote_user.py` 创建两个远程账号，启动 `bin/im_server.exe --config server/config/local.json`。两个窗口都进入 TCP 页面，连接 `127.0.0.1:56000`，本机测试取消 TLS，再以各自远程账号登录。第二个窗口仍使用 `启动TCP第二端.cmd` 的独立本地存档。局域网和公网需要启用 TLS；完整操作见 [中心服务器与双端测试](中心服务器与双端测试.md)。

本地好友 / 群聊属于课程模拟消息库，通过切换账号演示收发；TCP 页提供真实网络会话，消息只在该次连接中显示。

## 功能页面

| 页面 | 操作 |
|---|---|
| 会话 | 私聊、群聊、未读数、回执、搜索、收藏筛选、@成员与全体、撤回、导出 |
| 好友 | 好友与用户搜索、发送申请、备注分组、删除、拉黑、共同好友、跨服务推荐 |
| 群组 | 创建、加入申请、邀请、成员列表、管理员、禁言、移出、转让、子群、公告与容量、动态模式、退出与解散 |
| 通知 | 好友申请收发记录、同意 / 拒绝 / 撤销、入群审批 |
| 账号 | 个人资料、服务列表、开通与关闭、昵称密码、微信绑定 QQ |
| 数据 | 数量统计、服务分布、保存、数据目录、导出独立副本 |
| TCP 聊天 | 异步连接中心服务器、远程身份认证、在线私聊、发送确认、自动重连、TLS，自动识别本机 IPv4 |
| 装扮与会员 | 主题色、免费 / 会员气泡、自定义头像背景、头像框、小火人、四档权益、充值订单 |

各操作继续服从业务层权限和服务依赖规则。输入不合法时会显示具体原因，表单保留输入以便修正。业务操作立即自动保存；失败时保留内存修改，可在数据页重试。

桌面版本 4.0 的装扮、会员与充值操作见 [会员与装扮使用说明](会员与装扮使用说明.md)。会员和装扮信息保存在业务存档旁的 `.studio.json`，导入图片保存在 `.media` 目录；完整迁移需一起复制。装扮与会员保存失败时不会提交内存变更，可排除写入故障后重新提交。

## 打开与修改源码

双击 `打开Qt项目.cmd`。Qt Creator 读取 `CMakePresets.json` 中的 `qt-desktop`，自动生成该项目的 Qt / MSVC 配置。首次打开时若出现配置页，勾选 `qt-desktop` 并点击 Configure Project。选择运行目标 `im_desktop`。

源码位于 `gui/`。界面采用 C++ Qt Widgets 布局，配色由 `ThemeHsx.cpp` 集中维护。Designer 可用于后续添加 `.ui` 表单，当前界面没有依赖 `.ui` 文件。

新版前端使用 `LandscapeHsx` 绘制嵌入的高清山水图、圆角及渐变，`IconHsx` 绘制矢量路径图标，`ConversationDelegateHsx` 绘制会话卡片。默认背景通过 Qt 资源嵌入，无需外部文件。图像素材、配色与生成提示词见 [界面设计说明](界面设计说明.md)。

登录页点击“用户登录 / 管理空间”切换身份，密码右侧眼睛按钮控制显隐。会话左侧搜索框筛选联系人或最后一条消息，右侧搜索框检索当前会话历史。聊天区按日期分隔，支持原有回执、收藏、复制及撤回。

预设中的编译器路径对应当前电脑。迁移工程或升级 MSVC 后，可修改 `CMakePresets.json` 的 `vendor/qt.io/QtCreator/1.0/compiler` 路径。Qt Creator 设置单独保存在 `.tools/QtCreatorSettings`。

## 重新构建与验证

在工程目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_gui.ps1 -Test
```

脚本完成 CMake 配置、Release 编译、windeployqt 部署、核心及管理员测试和 GUI 测试，并同步更新控制台可执行文件。GUI 测试生成独立存档、`results.json`、登录页、七个普通页面和管理员后台截图，位于 `tests/gui-output/时间目录`。Qt GUI 没有安装时，原控制台工程仍可单独构建。

交付运行文件是 `bin` 中的 exe、Qt DLL 和插件目录。复制运行文件时应保留整个 bin 目录。其它电脑需要对应的 Microsoft Visual C++ 运行库。

## 环境复现

当前电脑已经完成安装，无需重新下载。需要重建工具目录时，可用 Python 执行：

```text
python scripts/setup_qt.py bootstrap
python scripts/setup_qt.py sdk
python scripts/setup_qt.py multimedia
python scripts/setup_qt.py creator
python scripts/setup_qt.py sdktool
python scripts/configure_creator.py
python scripts/build_gui.py --test
```

安装使用 aqtinstall 从 Qt 官方仓库获取 SDK 与 Creator。开发工具目录体积较大，不应纳入源码提交。

参考：[Qt Windows 部署](https://doc.qt.io/qt-6/windows-deployment.html)、[Qt Creator CMake 预设](https://www.qt.io/blog/qt-creator-20-cmake-update)、[SDKTool](https://doc.qt.io/qtcreator/creator-sdk-tool.html)。
