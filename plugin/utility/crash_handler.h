#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#include <ctime>
#include <stdio.h>


bool writeMinidump(EXCEPTION_POINTERS* ep);
LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* ep);
void installCrashHook();
#endif