#if defined(_WIN32)

#include <windows.h>
#include <dbghelp.h>
#include <ctime>
#include <stdio.h>
#include <shlobj.h>
#include <string>
#include <filesystem>


std::string getSafeCrashDumpFolder()
{
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        std::string folder = std::string(path) + "\\ysfx_crashes\\Crashes\\";
        std::filesystem::create_directories(folder);
        return folder;
    } else {
        return ".\\crashfallback\\";
    }
}


bool writeMinidump(EXCEPTION_POINTERS* ep) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    char filename[MAX_PATH];
    sprintf_s(filename, "plugin_crash_%04d%02d%02d_%02d%02d%02d.dmp",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::string fullPath = getSafeCrashDumpFolder() + filename;

    HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo);

    BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                hFile, dumpType, &mei, nullptr, nullptr);
    CloseHandle(hFile);
    return ok == TRUE;
}

/*
LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    writeMinidump(pExceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}
*/


LONG WINAPI MyUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo) {
    writeMinidump(pExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}


void installCrashHook()
{
    static bool installed = false;
    if (!installed)
    {
        // AddVectoredExceptionHandler(1, vectoredHandler);
        SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
        installed = true;
    }
}
#endif
