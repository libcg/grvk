#ifndef LOGGER_H_
#define LOGGER_H_

typedef enum {
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE,
} LogLevel;

#define LOGV(...) logPrint(LOG_LEVEL_VERBOSE, __func__, __VA_ARGS__)
#define LOGD(...) logPrint(LOG_LEVEL_DEBUG, __func__, __VA_ARGS__)
#define LOGI(...) logPrint(LOG_LEVEL_INFO, __func__, __VA_ARGS__)
#define LOGW(...) logPrint(LOG_LEVEL_WARNING, __func__, __VA_ARGS__)
#define LOGE(...) logPrint(LOG_LEVEL_ERROR, __func__, __VA_ARGS__)

void logInit();

void logPrint(
    LogLevel level,
    const char* name,
    const char* format,
    ...);

void logPrintRaw(
    const char* format,
    ...);

#endif // LOGGER_H_
