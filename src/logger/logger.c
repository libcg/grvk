#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "logger.h"

LogLevel gLogLevel = LOG_LEVEL_INFO;
static FILE* mLogFile = NULL;
static SRWLOCK mLogLock = SRWLOCK_INIT;

static void pickLogLevel()
{
    const char* levelNames[] = { "trace", "verbose", "debug", "info", "warning", "error", "none" };
    char* envValue = getenv("GRVK_LOG_LEVEL");

    if (envValue != NULL) {
        for (int i = LOG_LEVEL_TRACE; i <= LOG_LEVEL_NONE; i++) {
            if (strcmp(envValue, levelNames[i]) == 0) {
                gLogLevel = i;
                break;
            }
        }
    }
}

static const char* pickLogFile(
    const char* logPathEnv,
    const char* logPath)
{
    char* envValue = getenv(logPathEnv);

    if (envValue != NULL) {
        if (strlen(envValue) == 0) {
            return NULL;
        } else {
            return envValue;
        }
    }

    return logPath;
}

void logInit(
    const char* logPathEnv,
    const char* logPath)
{
    pickLogLevel();
    const char* path = pickLogFile(logPathEnv, logPath);

    if (gLogLevel != LOG_LEVEL_NONE && path != NULL) {
        mLogFile = fopen(path, "w");
    }
}

void logPrint(
    LogLevel level,
    const char* name,
    const char* format,
    ...)
{
    const char* prefixes[] = { "T", "V", "D", "I", "W", "E", "" };
    unsigned threadId = GetCurrentThreadId();

    AcquireSRWLockExclusive(&mLogLock);

    fprintf(stdout, "%s/%08X/%s: ", prefixes[level], threadId, name);
    if (mLogFile != NULL) {
        fprintf(mLogFile, "%s/%08X/%s: ", prefixes[level], threadId, name);
    }

    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    if (mLogFile != NULL) {
        vfprintf(mLogFile, format, argptr);
        fflush(mLogFile);
    }
    va_end(argptr);

    ReleaseSRWLockExclusive(&mLogLock);
}

void logPrintRaw(
    const char* format,
    ...)
{
    if (gLogLevel == LOG_LEVEL_NONE) {
        return;
    }

    AcquireSRWLockExclusive(&mLogLock);

    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    if (mLogFile != NULL) {
        vfprintf(mLogFile, format, argptr);
        fflush(mLogFile);
    }
    va_end(argptr);

    ReleaseSRWLockExclusive(&mLogLock);
}

