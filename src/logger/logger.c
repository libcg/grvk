#include <stdio.h>
#include <stdarg.h>
#include "logger.h"
void logPrint(
    LogLevel level,
    const char* name,
    const char* format,
    ...)
{
    const char* prefixes[] = { "V", "D", "I", "W", "E" };
    fprintf(stdout, "%s/%s: ", prefixes[level], name);

    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}

void logPrintRaw(
    const char* format,
    ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}

