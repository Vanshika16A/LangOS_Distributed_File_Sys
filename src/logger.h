#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <string.h>

// Log levels
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

// Function to log a message
static void log_message(LogLevel level, const char* source, const char* message) {
    time_t now = time(NULL);
    char time_buf[25];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    const char* level_str;
    switch (level) {
        case LOG_INFO:  level_str = "INFO";  break;
        case LOG_WARN:  level_str = "WARN";  break;
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_DEBUG: level_str = "DEBUG"; break;
        default:        level_str = "LOG";   break;
    }

    // Log to standard output (terminal)
    printf("[%s] [%s] [%s] %s\n", time_buf, source, level_str, message);
    fflush(stdout);
}

#endif // LOGGER_H