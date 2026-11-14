/* Release diagnostics mirror stdout/stderr without changing the bot's protocol. */
#ifndef PORT_LOG_H
#define PORT_LOG_H
#include <stdio.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
void Port_LogInit(const char *base);
const char *Port_LogPath(int online);
int Port_LogPrintf(const char *source, FILE *stream, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
int Port_LogVPrintf(const char *source, FILE *stream, const char *format, va_list args);
int Port_LogPuts(const char *source, const char *text, FILE *stream);
int Port_LogLine(const char *source, const char *text);
#ifdef __cplusplus
}
#endif
#if defined(PORT_DISTRIBUTION_BUILD) && PORT_DISTRIBUTION_BUILD && !defined(PORT_LOG_IMPLEMENTATION)
#define fprintf(stream, ...) Port_LogPrintf(__FILE__, stream, __VA_ARGS__)
#define vfprintf(stream, format, args) Port_LogVPrintf(__FILE__, stream, format, args)
#define printf(...) Port_LogPrintf(__FILE__, stdout, __VA_ARGS__)
#define fputs(text, stream) Port_LogPuts(__FILE__, text, stream)
#define puts(text) Port_LogLine(__FILE__, text)
#endif
#endif
