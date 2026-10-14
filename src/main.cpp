#include "ConsoleHsx.h"
#include "StorageHsx.h"
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

template<class Character> int application(int argc, Character* argv[]) {
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    try {
        // 默认存档始终放在程序旁；从VS、桌面或其它目录启动都使用同一份数据。
        auto file = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path() / "data" / "tencent_data.txt";
        std::filesystem::path imported;
        for (int i = 1; i < argc; ++i) {
            const auto option = std::filesystem::path(argv[i]).u8string();
            if (option == "--help") { std::cout << "用法：im_system [--data 存档路径] [--import 旧存档路径]\n导入要求目标存档不存在，成功后自动退出。\n"; return 0; }
            if ((option == "--data" || option == "--import") && i + 1 < argc) {
                const auto path = std::filesystem::path(argv[++i]); if (option == "--data") file = path; else imported = path;
            } else throw std::runtime_error("参数无效，使用 --help 查看用法");
        }
        im::DataStoreHsx data;
        if (!imported.empty()) {
            im::require(!std::filesystem::exists(file), "导入目标已存在，请指定新的--data路径");
            // 导入只读取原存档；旧版本备份只在原地升级时生成。
            im::StorageHsx::load(data, imported, false); im::StorageHsx::save(data, file);
            std::cout << "导入成功：用户 " << data.users().size() << " / 群 " << data.groups().size() << " / 消息 " << data.messages().size() << '\n'; return 0;
        }
        if (std::filesystem::exists(file)) im::StorageHsx::load(data, file);
        else im::StorageHsx::save(data, file);
        im::ConsoleHsx(data, file).run(); return data.dirty() ? 1 : 0;
    } catch (const std::exception& error) { std::cerr << "启动失败：" << error.what() << '\n'; return 1; }
}
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) { SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8); return application(argc, argv); }
#else
int main(int argc, char* argv[]) { return application(argc, argv); }
#endif
