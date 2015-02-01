#ifndef __EBDR_LOGGING_H
#define __EBDR_LOGGING_H
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "atdr_daemon.h"

#define EBDR_FATAL   1
#define EBDR_ERROR   2
#define EBDR_WARN    3
#define EBDR_INFO    4
#define EBDR_DEBUG   5

#define LEVEL 5

#define ebdr_log(prio,  msg, ...) do {\
	int logging_level = thread_params[log_pid].logging_level; \
	FILE *ebdrlog_fp = thread_params[log_pid].ebdrlog_fp; \
	char *logStrng = thread_params[log_pid].logStrng; \
if (prio < logging_level || prio == logging_level)\
if (prio == EBDR_FATAL)\
	logStrng = "FATAL"; \
else if (prio == EBDR_ERROR)\
	logStrng = "ERR"; \
else if (prio == EBDR_WARN)\
	logStrng = "WARN"; \
else if (prio == EBDR_INFO)\
	logStrng = "INFO"; \
else if (prio == EBDR_DEBUG)\
	logStrng = "DEBUG"; \
if (strcmp(logStrng, "temp") != 0) {\
	printf(" %s : %d : %s : %s : [%s]: "msg" ", \
	__FILE__, __LINE__, __DATE__, __TIME__, logStrng, ##__VA_ARGS__); \
	fprintf(ebdrlog_fp, " %s : %d : %s : %s [%s]: "msg" ", \
	__FILE__, __LINE__, __DATE__, __TIME__, logStrng, ##__VA_ARGS__); \
	fflush(ebdrlog_fp); \
}\
} while (0)
#endif
