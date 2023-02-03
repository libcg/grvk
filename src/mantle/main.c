#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "logger.h"
#include "version.h"

BOOLEAN WINAPI DllMain(
    HINSTANCE hDllHandle,
    DWORD nReason,
    LPVOID Reserved)
 {
     if (nReason == DLL_PROCESS_ATTACH) {
        logInit("GRVK_LOG_PATH", "grvk.log");
        logPrintRaw("=== GRVK %s ===\n", GRVK_VERSION);
     }

     return TRUE;
 }
