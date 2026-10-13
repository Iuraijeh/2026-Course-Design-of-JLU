#ifndef CONSOLEUTIL_HEJR_H
#define CONSOLEUTIL_HEJR_H

// ============================================================
// 控制台编码工具 ConsoleUtil_Hejr
// 说明：
//   Windows 控制台默认使用 GBK（代码页 936）编码，
//   而本项目源文件以 UTF-8 保存，导致中文输出乱码。
//   本文件提供 initConsole() 函数，在 main 开头调用一次即可：
//     1. 设置控制台输出代码页为 UTF-8（65001）
//     2. 设置控制台输入代码页为 UTF-8（65001）
//   类名后缀 _Hejr 为姓名缩写占位符，请替换。
// ============================================================

#ifdef _WIN32
  #include <windows.h>
#endif

class ConsoleUtil_Hejr
{
public:
    // 在 main 函数最开头调用一次，解决中文乱码
    static void initConsole()
    {
#ifdef _WIN32
        // 设置控制台输出代码页为 UTF-8
        SetConsoleOutputCP(65001);
        // 设置控制台输入代码页为 UTF-8
        SetConsoleCP(65001);
        // 备用方案：通过 chcp 命令设置（若上面 API 不生效）
        // system("chcp 65001 > nul");
#endif
    }
};

#endif // CONSOLEUTIL_HEJR_H


