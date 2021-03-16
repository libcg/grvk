#ifndef LOGGER_H_
#define LOGGER_H_

#define LOG(level, ...) { if ((level) >= gLogLevel) { logPrint((level), __func__, __VA_ARGS__); } }
#define LOGT(...) LOG(LOG_LEVEL_TRACE, __VA_ARGS__)
#define LOGV(...) LOG(LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define LOGD(...) LOG(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOGI(...) LOG(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOGW(...) LOG(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOGE(...) LOG(LOG_LEVEL_ERROR, __VA_ARGS__)

typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE,
} LogLevel;

extern LogLevel gLogLevel;

void logInit(
    const char* logPathEnv,
    const char* logPath);

void logPrint(
    LogLevel level,
    const char* name,
    const char* format,
    ...);

void logPrintRaw(
    const char* format,
    ...);

#endif // LOGGER_H_
