#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "logger.h"

BOOLEAN WINAPI DllMain(
    HINSTANCE hDllHandle,
    DWORD nReason,
    LPVOID Reserved)
 {
     if (nReason == DLL_PROCESS_ATTACH) {
        logInit("GRVK_AXL_LOG_PATH", "grvk_axl.log");
        logPrintRaw("=== GRVK %s (AXL) ===\n", GRVK_VERSION);
     }

     return TRUE;
 }
