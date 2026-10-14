# GitHub 与自动更新准备

源码仓库与运行数据分开管理。GitHub 保存 C++/Qt 源码、构建脚本、测试、模板配置和界面资源；真实数据库、账号/聊天/订单、测试证书、私钥、收款二维码、SDK 和生成的 EXE 不进入 Git。

## 日常同步

用户已授权每次本项目更新完成并验证后自动同步 GitHub。这项要求记录在仓库 `AGENTS.md`，以后项目任务在收尾时执行，无需重复确认。同步后核对远端提交；认证或网络失败时明确报告。本约定不创建定时任务，不上传真实数据库、私钥、个人配置或发行 EXE。

在仓库目录修改源码后：

```powershell
git status --short
git diff
git add src gui network server workspace scripts tests docs CMakeLists.txt
git commit -m "描述本次修改"
git push origin main
```

GitHub 插件授权和本机 Git 认证是两套连接。若命令行 push 提示认证，请通过 Git Credential Manager 的官方浏览器登录流程完成；不要把密码或令牌写到仓库或发到聊天里。

仓库中的 GitHub Actions 在推送、拉取请求或手动启动时编译 Windows 客户端和服务端、运行核心测试，并编译 Ubuntu 独立服务端。工作流只请求 `contents: read`；不会部署服务器、修改数据库、自动发布安装包或执行真实支付。

Action 说明：[actions/checkout](https://github.com/actions/checkout)、[install-qt-action](https://github.com/jurplel/install-qt-action)。首次远程运行的成功状态需到仓库 Actions 页面确认，本机编译成功不能代替远程结果。

## 版本发布

1. 确认新功能通过测试，更新 `VERSION`、`CHANGELOG.md` 和当前使用版本的构建/启动入口。
2. 在受信任的 Windows 构建环境执行现有 `scripts/package_windows.py`，生成便携 EXE、安装包、SHA256SUMS。
3. 通过 GitHub Release 发布正式产物，标签格式 `v5.3.1`。EXE 使用 Release 资产管理，避免把每次构建的二进制塞进源码 Git 历史。
4. 发布前核对产物不含真实账号库、测试用户数据、服务端私钥；根据部署者的支付和授权规则审查图片资源。

Windows 打包脚本目前使用项目本地 Qt/工具路径，尚未完成无需本机工具目录的云端打包；现有 CI 是编译基础，不代表自动发行流程已经完成。

## 客户端自动更新的下一阶段

本阶段准备了源码管理、版本记录与 CI，**尚未实现客户端自动下载安装**。后续应增加版本检查、下载与安装流程，更新时保留 `%LOCALAPPDATA%` 中的数据库和个人配置。

建议更新元数据至少包含：版本号、对应平台、最低兼容版本、下载地址、产物大小、SHA256、发行说明和签名。下载必须使用 HTTPS，验证哈希与发行签名；Windows 退出主程序后再运行经过校验的安装包，避免覆盖正在使用的 EXE。

当前仓库如果为私有，普通客户端无法匿名读取私有 Release。不要把 GitHub PAT 或管理员凭据硬编码进客户端；将来可用中心服务器提供更新信息和授权下载，或独立维护公开的发行渠道。服务端数据库迁移应单独备份、验证与回滚，不能由客户端更新覆盖。

## 本机资源保留

`.gitignore` 只控制上传，不删除文件。当前电脑的数据库、证书、配置、收款图片和发行包仍保留。新电脑构建默认使用收款占位图；如需真实收款功能，在 `assets/payment/wechat-receive.jpg` 放入该部署者自己的图片后重新配置 CMake。
