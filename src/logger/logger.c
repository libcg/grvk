#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"

static LogLevel mLogLevel = LOG_LEVEL_INFO;

void logInit()
{
    const char* levelNames[] = { "verbose", "debug", "info", "warning", "error", "none" };
    char* envValue = getenv("GRVK_LOG_LEVEL");

    if (envValue != NULL) {
        for (int i = LOG_LEVEL_VERBOSE; i <= LOG_LEVEL_NONE; i++) {
            if (strcmp(envValue, levelNames[i]) == 0) {
                mLogLevel = i;
                break;
            }
        }
    }
}

void logPrint(
    LogLevel level,
    const char* name,
    const char* format,
    ...)
{
    if (level < mLogLevel) {
        return;
    }

    const char* prefixes[] = { "V", "D", "I", "W", "E", "" };
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
    if (mLogLevel == LOG_LEVEL_NONE) {
        return;
    }

    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}

