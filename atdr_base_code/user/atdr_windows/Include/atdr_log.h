#ifndef __ATDR_LOGGING_H
#define __ATDR_LOGGING_H
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "atdr_daemon.h"

#define ATDR_FATAL   1
#define ATDR_ERROR   2
#define ATDR_WARN    3
#define ATDR_INFO    4
#define ATDR_DATUG   5

#define LEVEL 5

#define atdr_log(prio,  msg, ...) do {\
	int logging_level = thread_params[log_pid].logging_level; \
	FILE *atdrlog_fp = thread_params[log_pid].atdrlog_fp; \
	char *logStrng = thread_params[log_pid].logStrng; \
if (prio < logging_level || prio == logging_level)\
if (prio == ATDR_FATAL)\
	logStrng = "FATAL"; \
else if (prio == ATDR_ERROR)\
	logStrng = "ERR"; \
else if (prio == ATDR_WARN)\
	logStrng = "WARN"; \
else if (prio == ATDR_INFO)\
	logStrng = "INFO"; \
else if (prio == ATDR_DATUG)\
	logStrng = "DATUG"; \
if (strcmp(logStrng, "temp") != 0) {\
	printf(" %s : %d : %s : %s : [%s]: "msg" ", \
	__FILE__, __LINE__, __DATE__, __TIME__, logStrng, ##__VA_ARGS__); \
	fprintf(atdrlog_fp, " %s : %d : %s : %s [%s]: "msg" ", \
	__FILE__, __LINE__, __DATE__, __TIME__, logStrng, ##__VA_ARGS__); \
	fflush(atdrlog_fp); \
}\
} while (0)
#endif
