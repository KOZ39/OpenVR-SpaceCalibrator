#include "crash_handler.h"
#if OS_WINDOWS
// clang-format off
#include "util.h"
#include <windows.h>
#include <Dbghelp.h>
// clang-format on

namespace spacecal {
    // based on https://mecanik.dev/en/posts/how-to-write-mini-dump-on-software-crash/

    LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = 0;
    wchar_t g_dumpsDirW[MAX_PATH] = { 0 };

    LONG WINAPI generate_minidump_handler(EXCEPTION_POINTERS* info) {
        wchar_t dumpPath[MAX_PATH] = { 0 };

        SYSTEMTIME systemTime = {};
        GetLocalTime(&systemTime);

        wsprintfW(dumpPath, L"%s\\SpaceCalibrator-overlay-%04d_%02d_%02d_%02d_%02d_%02d.dmp", g_dumpsDirW, systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

        HANDLE file = CreateFileW(dumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION minidumpExceptionInfo = {
                .ThreadId = (DWORD)GetCurrentThreadId(),
                .ExceptionPointers = info,
                .ClientPointers = 0,
            };

            if (MiniDumpWriteDump(GetCurrentProcess(), (DWORD)GetCurrentProcessId(), file, (MINIDUMP_TYPE)(MiniDumpScanMemory + MiniDumpWithIndirectlyReferencedMemory), &minidumpExceptionInfo, 0, 0) != 0) {
                CloseHandle(file);
                return EXCEPTION_EXECUTE_HANDLER;
            }
        }

        CloseHandle(file);

        return EXCEPTION_CONTINUE_SEARCH;
    }

    // @NOTE: we copy path to static memory because we want to avoid using the heap in any capacity IF we crash ; we do NOT know what
    //        state the program is and how reliable any operations are so we should minimise everything which MAY cause a crash
    void init_crash_handler() {
        SetErrorMode(SEM_FAILCRITICALERRORS);
        wcsncpy_s(g_dumpsDirW, util::getSpaceCalibratorDumpsDir().wstring().c_str(), _TRUNCATE);
        g_previousExceptionFilter = SetUnhandledExceptionFilter(generate_minidump_handler);
    }

    void shutdown_crash_handler() {
        SetUnhandledExceptionFilter(g_previousExceptionFilter);
    }
}
#endif // OS_WINDOWS