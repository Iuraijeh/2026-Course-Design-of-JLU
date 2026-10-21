// ============================================================
// main_Hejr.cpp — 模拟即时通信系统 主入口
// 说明：
//   - 实例化 Platform_Hejr，构造时自动加载数据文件
//     （users.txt / groups.txt / friends_<userId>.txt）
//   - 首次运行自动初始化 3 个测试用户及好友关系
//   - 进入主菜单循环，退出时析构自动保存（断电保存）
//   - 类名/文件名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================

#if __has_include("ConsoleUtil_Hejr.h")
#include "ConsoleUtil_Hejr.h"
#else
#include "ConsoleUtil.h"
#endif
#include "Platform_Hejr.h"
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

int main()
{
    // 桌面上的 design-N.exe 仍使用 Design 目录中的课程数据。
#ifdef _WIN32
    if (!std::filesystem::exists("users.txt") ||
        !std::filesystem::exists("groups.txt"))
    {
        wchar_t executablePath[MAX_PATH] = {};
        if (GetModuleFileNameW(NULL, executablePath, MAX_PATH) != 0)
        {
            std::filesystem::path dataDir =
                std::filesystem::path(executablePath).parent_path() / "Design";
            if (std::filesystem::exists(dataDir / "users.txt") &&
                std::filesystem::exists(dataDir / "groups.txt"))
                std::filesystem::current_path(dataDir);
        }
    }
#endif
    // 在所有输出之前，先将控制台切换为 UTF-8 编码
    ConsoleUtil_Hejr::initConsole();

    Platform_Hejr platform;
    platform.run();
    return 0;
}

